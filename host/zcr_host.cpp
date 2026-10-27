/* zcr-host: arm64 replacement for the original libZombieCafeAndroid.so.
 *
 * ART loads this library (same soname) and resolves the same Java_* symbols.
 * Each entry point marshals its arguments and forwards into the emulated
 * 32-bit engine. Performance notes:
 *   - code hook is ranged over the two stub pages only (not all code)
 *   - engine image + heap are uc_mem_map_ptr zero-copy aliases
 *   - TCG buffer is enlarged via uc_ctl */
#include <jni.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <unicorn/unicorn.h>

#include "emu_mem.h"
#include "engine_loader.h"
#include "engine_memory.h"
#include "imports_gl.h"
#include "imports_posix.h"
#include "jni_bridge.h"
#include "stub_dispatch.h"
#include "generated/engine_tables.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "zcr-host", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "zcr-host", __VA_ARGS__)

namespace {

uc_engine *g_uc = nullptr;
EngineImage g_img;
JavaVM *g_vm = nullptr;

/* ---- code hook: only fires on stub pages (ranged) ---- */
bool on_code(uc_engine *uc, uint32_t address, uint32_t, void *) {
    if (address >= JNI_STUB_PAGE && address < JNI_STUB_PAGE + 0x10000) {
        /* JNIEnv function trap */
        uint32_t idx = (address - JNI_STUB_PAGE) / STUB_SLOT_SIZE;
        char nm[64];
        snprintf(nm, sizeof(nm), "JNIEnv#%u", idx);
        emu::Args args;
        args.uc = uc;
        uc_reg_read(uc, UC_ARM_REG_R0, &args.r[0]);
        uc_reg_read(uc, UC_ARM_REG_R1, &args.r[1]);
        uc_reg_read(uc, UC_ARM_REG_R2, &args.r[2]);
        uc_reg_read(uc, UC_ARM_REG_R3, &args.r[3]);
        uc_reg_read(uc, UC_ARM_REG_SP, &args.sp);
        uint32_t lr = 0, rv = 0;
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        if (!stubs_dispatch_by_name(uc, nm, args, &rv))
            LOGE("unimplemented JNIEnv fn #%u", idx);
        uc_reg_write(uc, UC_ARM_REG_R0, &rv);
        emu_set_pc(uc, lr);
        return true;
    }
    if (address >= STUB_PAGE && address < STUB_PAGE + 0x10000) {
        StubResult r = stubs_dispatch(uc, address);
        if (r == StubResult::kNeedsDriver) {
            uc_emu_stop(uc);
            return true;
        }
        return true;
    }
    return true;
}

bool load_engine_from_assets(JNIEnv *env) {
    jclass atCls = env->FindClass("android/app/ActivityThread");
    jmethodID curApp = env->GetStaticMethodID(atCls, "currentApplication",
                                              "()Landroid/app/Application;");
    jobject app = env->CallStaticObjectMethod(atCls, curApp);
    if (!app) return false;
    jclass ctxCls = env->FindClass("android/content/Context");
    jmethodID getAssets = env->GetMethodID(ctxCls, "getAssets",
                                           "()Landroid/content/res/AssetManager;");
    jobject am = env->CallObjectMethod(app, getAssets);
    AAssetManager *mgr = AAssetManager_fromJava(env, am);
    if (!mgr) return false;

    AAsset *a = AAssetManager_open(mgr, "engine/game.so", AASSET_MODE_BUFFER);
    if (!a) {
        LOGE("asset engine/game.so missing");
        return false;
    }
    size_t sz = (size_t)AAsset_getLength(a);
    g_img.bytes.resize(sz);
    memcpy(g_img.bytes.data(), AAsset_getBuffer(a), sz);
    AAsset_close(a);
    LOGI("engine image: %zu bytes", sz);
    return true;
}

bool host_init(JNIEnv *env) {
    if (g_uc) return true;

    if (!load_engine_from_assets(env)) return false;

    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &g_uc);
    if (err != UC_ERR_OK) {
        LOGE("uc_open: %s", uc_strerror(err));
        return false;
    }
    emu::set_engine(g_uc);

    /* larger TCG buffer = fewer codegen flushes */
    size_t tcg = 32u * 1024u * 1024u;
    uc_ctl(g_uc, UC_CTL_WRITE(UC_CTL_TCG_BUFFER_SIZE, 1), &tcg);

    /* zero-copy host buffers for stack/heap + trap pages */
    void *heap_buf = calloc(1, HEAP_SIZE);
    static uint8_t *stack_buf = (uint8_t *)calloc(1, STACK_SIZE);
    if (!emu::heap_init(heap_buf)) return false;
    emu::map_alias(STACK_BASE, STACK_SIZE, stack_buf, true);
    uc_mem_map(g_uc, STUB_PAGE, 0x10000, UC_PROT_ALL);
    uc_mem_map(g_uc, JNI_STUB_PAGE, 0x10000, UC_PROT_ALL);
    uc_mem_map(g_uc, JNI_ENV_OBJ, 0x1000, UC_PROT_ALL);
    uc_mem_map(g_uc, EMU_RETURN_ADDR & ~0xFFFu, 0x1000, UC_PROT_ALL);

    if (!engine_map(g_uc, g_img)) return false;
    engine_apply_zcr_patches(g_uc);

    stubs_init();
    register_posix_imports();
    register_gl_imports();
    jni_bridge_init(g_uc, g_vm);

    /* ranged code hooks: only the two trap pages */
    uc_hook h1, h2;
    uc_hook_add(g_uc, &h1, UC_HOOK_CODE, (void *)on_code, nullptr, STUB_PAGE,
                STUB_PAGE + 0x10000);
    uc_hook_add(g_uc, &h2, UC_HOOK_CODE, (void *)on_code, nullptr,
                JNI_STUB_PAGE, JNI_STUB_PAGE + 0x10000);

    /* run the engine's own JNI_OnLoad */
    for (uint32_t i = 0; i < ENGINE_JNI_COUNT; i++) {
        if (strcmp(kEngineJniExports[i].name, "JNI_OnLoad") == 0) {
            uint32_t vm = JAVA_VM_OBJ;
            jni_call_engine(g_uc, kEngineJniExports[i].rva, &vm, 1);
            break;
        }
    }
    LOGI("host initialized");
    return true;
}

uint32_t engine_rva(const char *name) {
    for (uint32_t i = 0; i < ENGINE_JNI_COUNT; i++)
        if (strcmp(kEngineJniExports[i].name, name) == 0)
            return kEngineJniExports[i].rva;
    return 0;
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    g_vm = vm;
    JNIEnv *env = nullptr;
    vm->GetEnv((void **)&env, JNI_VERSION_1_6);
    host_init(env);
    return JNI_VERSION_1_6;
}

/* ------------------------------------------------------------------
 * Java_* forwarders. Conventions:
 *   words[0]        = handle for `thiz`/class
 *   words[1..]      = primitive args (ints / float bits / handles)
 * Strings and arrays are converted to emulated memory buffers whose
 * addresses are passed as handles (the engine reads them via JNI).
 * ---------------------------------------------------------------- */
namespace {

uint32_t str_word(JNIEnv *env, jstring s) {
    /* pass the real jstring as a handle – the engine reads it through the
       JNI vtable (GetStringUTFChars), not as a raw pointer */
    (void)env;
    return jni_handle_for(s);
}

uint32_t bytes_word(JNIEnv *env, jbyteArray arr) {
    (void)env;
    return jni_handle_for(arr);
}

uint32_t floats_word(JNIEnv *env, jfloatArray arr) {
    (void)env;
    return jni_handle_for(arr);
}

float fbits(jfloat f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return *(float *)&u;
}
uint32_t jf(jfloat f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

#define ENTRY(name)                                              \
    extern "C" JNIEXPORT jint JNICALL name(JNIEnv *env, jobject thiz)

#define GUARD()                                        \
    if (!host_init(env)) return 0;                     \
    uint32_t w[8];                                     \
    uint32_t nw = 0;                                   \
    w[nw++] = jni_handle_for(thiz)

#define CALL(n) \
    return (jint)jni_call_engine(g_uc, engine_rva(#n), w, nw)

}  // namespace

/* --- no-arg / simple --- */
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_CapcomRenderer_render(JNIEnv *env, jclass cls,
                                                       jint frame) {
    if (!host_init(env)) return 0;
    uint32_t w[2] = {jni_handle_for(cls), (uint32_t)frame};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_CapcomRenderer_render"),
                                 w, 2);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_InitMethodCalls) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_InitMethodCalls);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_pause) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_pause);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_deviceShaken) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_deviceShaken);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_HandleBackButton) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_HandleBackButton);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_DialogCallBack) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_DialogCallBack);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_ClearDialogFlag) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_ClearDialogFlag);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_CheckDoneLoading) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_CheckDoneLoading);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_CheckIfInHelpScreen) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_CheckIfInHelpScreen);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_OnFriendScreen) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_OnFriendScreen);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_StartNotifications) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_StartNotifications);
}
ENTRY(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_WatchedTrailer) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_WatchedTrailer);
}
ENTRY(Java_com_capcom_zombiecafeandroid_SmurfsGLSurfaceView_DebugGame) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_SmurfsGLSurfaceView_DebugGame);
}
ENTRY(Java_com_capcom_zombiecafeandroid_FacebookCallback) {
    GUARD();
    CALL(Java_com_capcom_zombiecafeandroid_FacebookCallback);
}

/* --- arg-bearing (true prototypes) --- */
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_SoundManager_setEnabled(JNIEnv *env,
                                                         jobject thiz,
                                                         jboolean on) {
    if (!host_init(env)) return 0;
    uint32_t w[2] = {jni_handle_for(thiz), (uint32_t)on};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_SoundManager_setEnabled"),
                                 w, 2);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_updateAccelerometer(
    JNIEnv *env, jclass cls, jfloat x, jfloat y, jfloat z) {
    if (!host_init(env)) return 0;
    uint32_t w[4] = {jni_handle_for(cls), jf(x), jf(y), jf(z)};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_updateAccelerometer"),
                                 w, 4);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_CapcomFacebook_onFacebook(JNIEnv *env,
                                                           jclass cls,
                                                           jboolean on) {
    if (!host_init(env)) return 0;
    uint32_t w[2] = {jni_handle_for(cls), (uint32_t)on};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_CapcomFacebook_onFacebook"),
                                 w, 2);
}

/* --- arg-bearing --- */

/* Because ART passes real arguments positionally, we must read them from the
 * JNI signature – but C varargs after (JNIEnv*, jobject) arrive in registers
 * already. We declare each entry with its true prototype below. */

extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_CapcomRenderer_CreateGame(JNIEnv *env,
                                                           jobject thiz,
                                                           jclass cls,
                                                           jint a, jint b,
                                                           jfloat c, jfloat d) {
    if (!host_init(env)) return 0;
    uint32_t w[8] = {jni_handle_for(thiz), jni_handle_for(cls), (uint32_t)a,
                     (uint32_t)b, jf(c), jf(d)};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_CapcomRenderer_CreateGame"),
                                 w, 6);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseDown(JNIEnv *env,
                                                             jclass cls,
                                                             jfloat x,
                                                             jfloat y, jint t) {
    if (!host_init(env)) return 0;
    uint32_t w[4] = {jni_handle_for(cls), jf(x), jf(y), (uint32_t)t};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseDown"),
                                 w, 4);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseUp(JNIEnv *env,
                                                           jclass cls, jfloat x,
                                                           jfloat y, jint t) {
    if (!host_init(env)) return 0;
    uint32_t w[4] = {jni_handle_for(cls), jf(x), jf(y), (uint32_t)t};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseUp"),
                                 w, 4);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseMove(JNIEnv *env,
                                                             jclass cls,
                                                             jfloatArray pts,
                                                             jint n) {
    if (!host_init(env)) return 0;
    uint32_t w[3] = {jni_handle_for(cls), floats_word(env, pts), (uint32_t)n};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseMove"),
                                 w, 3);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_PurchaseAndroidToxin(
    JNIEnv *env, jclass cls, jint amount) {
    if (!host_init(env)) return 0;
    uint32_t w[2] = {jni_handle_for(cls), (uint32_t)amount};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_PurchaseAndroidToxin"),
                                 w, 2);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_setDeviceModel(JNIEnv *env,
                                                                  jclass cls,
                                                                  jstring s) {
    if (!host_init(env)) return 0;
    uint32_t w[2] = {jni_handle_for(cls), str_word(env, s)};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_setDeviceModel"),
                                 w, 2);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_setVanityString(JNIEnv *env,
                                                                   jclass cls,
                                                                   jstring s) {
    if (!host_init(env)) return 0;
    uint32_t w[2] = {jni_handle_for(cls), str_word(env, s)};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_setVanityString"),
                                 w, 2);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_URLManager_LoadedBannerTextureCallback(
    JNIEnv *env, jclass cls, jbyteArray data, jint n) {
    if (!host_init(env)) return 0;
    uint32_t w[3] = {jni_handle_for(cls), bytes_word(env, data), (uint32_t)n};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_URLManager_LoadedBannerTextureCallback"),
                                 w, 3);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_URLManager_NewRequestCallback(JNIEnv *env,
                                                               jclass cls,
                                                               jbyteArray data,
                                                               jint n) {
    if (!host_init(env)) return 0;
    uint32_t w[3] = {jni_handle_for(cls), bytes_word(env, data), (uint32_t)n};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_URLManager_NewRequestCallback"),
                                 w, 3);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_URLManager_NewRequestServerCallback(
    JNIEnv *env, jclass cls, jboolean ok, jbyteArray data, jint a, jint b) {
    if (!host_init(env)) return 0;
    uint32_t w[5] = {jni_handle_for(cls), (uint32_t)ok, bytes_word(env, data),
                     (uint32_t)a, (uint32_t)b};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_URLManager_NewRequestServerCallback"),
                                 w, 5);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_NetworkTask_NewRequestServerCallback(
    JNIEnv *env, jclass cls, jboolean ok, jbyteArray data, jint a, jint b) {
    if (!host_init(env)) return 0;
    uint32_t w[5] = {jni_handle_for(cls), (uint32_t)ok, bytes_word(env, data),
                     (uint32_t)a, (uint32_t)b};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_NetworkTask_NewRequestServerCallback"),
                                 w, 5);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_NetworkTaskPost_NewRequestServerCallback(
    JNIEnv *env, jclass cls, jboolean ok, jbyteArray data, jint a, jint b) {
    if (!host_init(env)) return 0;
    uint32_t w[5] = {jni_handle_for(cls), (uint32_t)ok, bytes_word(env, data),
                     (uint32_t)a, (uint32_t)b};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_NetworkTaskPost_NewRequestServerCallback"),
                                 w, 5);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_CapcomFacebook_setFBInfo(
    JNIEnv *env, jclass cls, jstring a, jstring b, jstring c, jstring d) {
    if (!host_init(env)) return 0;
    uint32_t w[5] = {jni_handle_for(cls), str_word(env, a), str_word(env, b),
                     str_word(env, c), str_word(env, d)};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_CapcomFacebook_setFBInfo"),
                                 w, 5);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_CapcomFacebook_sendFriendInfo(
    JNIEnv *env, jclass cls, jint a, jint b, jstring c, jstring d, jstring e,
    jstring f, jstring g) {
    if (!host_init(env)) return 0;
    uint32_t w[8] = {jni_handle_for(cls), (uint32_t)a, (uint32_t)b,
                     str_word(env, c), str_word(env, d), str_word(env, e),
                     str_word(env, f), str_word(env, g)};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_CapcomFacebook_sendFriendInfo"),
                                 w, 8);
}

/* Smurfs renderer hooks (texture reload path) */
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_ReloadTask_reloadTextures(
    JNIEnv *env, jobject thiz) {
    GUARD();
    CALL(NAME);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_SmurfsRenderer_setTexture(JNIEnv *env,
                                                           jobject thiz,
                                                           jint id) {
    if (!host_init(env)) return 0;
    uint32_t w[2] = {jni_handle_for(thiz), (uint32_t)id};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_SmurfsRenderer_setTexture"),
                                 w, 2);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_capcom_zombiecafeandroid_SmurfsRenderer_resize(JNIEnv *env,
                                                       jobject thiz, jint a,
                                                       jint b) {
    if (!host_init(env)) return 0;
    uint32_t w[3] = {jni_handle_for(thiz), (uint32_t)a, (uint32_t)b};
    return (jint)jni_call_engine(g_uc,
                                 engine_rva("Java_com_capcom_zombiecafeandroid_SmurfsRenderer_resize"),
                                 w, 3);
}
