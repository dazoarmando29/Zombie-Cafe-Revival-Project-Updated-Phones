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
#include "diag.h"
#include "generated/engine_tables.h"

#include <GLES/gl.h>

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
        if (!stubs_dispatch_by_name(uc, nm, args, &rv)) {
            char un[80];
            snprintf(un, sizeof(un), "JNIEnv fn #%u", idx);
            diag_note_unimpl(un);
        }
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

/* ---- memory fault logging: pinpoint bad pointers the engine chases ---- */
bool on_mem_fault(uc_engine *uc, uc_mem_type type, uint64_t address,
                  int size, int64_t value, void *) {
    uint32_t pc = 0, lr = 0, ip = 0;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R12, &ip);
    uint32_t used_kb = 0, total_kb = 0, free_blocks = 0;
    emu::heap_stats(&used_kb, &total_kb, &free_blocks);
    diag_log("[mem-fault] type=%d addr=0x%llx size=%d pc=0x%x lr=0x%x ip=0x%x heap=%u/%uKB free=%u",
             (int)type, (unsigned long long)address, size, pc, lr, ip,
             used_kb, total_kb, free_blocks);
    /* full register state: r0=dst String, r1=src String in copy-ctor crashes */
    uint32_t r[16] = {0};
    static const int kRegs[16] = {
        UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
        UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
        UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
        UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC};
    for (int i = 0; i < 16; i++) uc_reg_read(uc, kRegs[i], &r[i]);
    diag_log("[regs] r0=0x%x r1=0x%x r2=0x%x r3=0x%x r4=0x%x r5=0x%x r6=0x%x r7=0x%x",
             r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
    diag_log("[regs] r8=0x%x r9=0x%x r10=0x%x r11=0x%x r12=0x%x sp=0x%x lr=0x%x pc=0x%x",
             r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
    /* where do the interesting pointers live? */
    char desc[256];
    emu::heap_describe((uint32_t)address, desc, sizeof(desc));
    diag_log("[fault-addr] %s", desc);
    emu::heap_describe(r[1], desc, sizeof(desc));
    diag_log("[r1-src] %s", desc);
    emu::heap_describe(r[0], desc, sizeof(desc));
    diag_log("[r0-dst] %s", desc);
    /* dump 3 words at r0 and r1 (String = {StringRef*, off16, len16}) */
    uint32_t w[4] = {0, 0, 0, 0};
    if (emu::read(uc, r[1], w, 16))
        diag_log("[r1-mem] %08x %08x %08x %08x", w[0], w[1], w[2], w[3]);
    if (emu::read(uc, r[0], w, 16))
        diag_log("[r0-mem] %08x %08x %08x %08x", w[0], w[1], w[2], w[3]);
    /* String-copy-ctor crash: walk the HudElement lookup chain
     *   game = [this(r4)+0x60]; fbc = [game+0xbc]; table = [fbc+8]
     *   id = [r2]  (r2 = pointer into the engine ID table) */
    if (pc == 0x10141bd4u || pc == 0x10141c42u) {
        uint32_t game = 0, fbc = 0, table = 0, id = 0;
        emu::read(uc, r[4] + 0x60, &game, 4);
        emu::read(uc, r[2], &id, 4);
        if (game) emu::read(uc, game + 0xbc, &fbc, 4);
        if (fbc) emu::read(uc, fbc + 8, &table, 4);
        diag_log("[strchain] game=0x%x fbc=0x%x table=0x%x id=%u idptr=0x%x",
                 game, fbc, table, id, r[2]);
        uint32_t cookie[3] = {0, 0, 0};
        if (table && emu::read(uc, table - 12, cookie, 12))
            diag_log("[strchain] cookie[-3..-1] %u %u %u", cookie[0], cookie[1], cookie[2]);
        char d2[256];
        emu::heap_describe(table, d2, sizeof(d2));
        diag_log("[strchain] table: %s", d2);
        emu::heap_describe(table ? table + id * 8 : 0, d2, sizeof(d2));
        diag_log("[strchain] table[id]: %s", d2);
        uint32_t tw[8] = {0};
        if (table && emu::read(uc, table, tw, 32))
            diag_log("[strchain] tbl[0..3] %08x %08x %08x %08x %08x %08x %08x %08x",
                     tw[0], tw[1], tw[2], tw[3], tw[4], tw[5], tw[6], tw[7]);
        if (table && emu::read(uc, table + (id > 2 ? (id - 2) * 8 : 0), tw, 32))
            diag_log("[strchain] tbl[id-2..] %08x %08x %08x %08x %08x %08x %08x %08x",
                     tw[0], tw[1], tw[2], tw[3], tw[4], tw[5], tw[6], tw[7]);
        if (emu::read(uc, r[1] > 24 ? r[1] - 24 : r[1], tw, 32))
            diag_log("[strchain] r1-24.. %08x %08x %08x %08x %08x %08x %08x %08x",
                     tw[0], tw[1], tw[2], tw[3], tw[4], tw[5], tw[6], tw[7]);
    }
    diag_dump_traces();
    return false;  /* stop emulation */
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
    uc_hook h1, h2, h3;
    uc_hook_add(g_uc, &h1, UC_HOOK_CODE, (void *)on_code, nullptr, STUB_PAGE,
                STUB_PAGE + 0x10000);
    uc_hook_add(g_uc, &h2, UC_HOOK_CODE, (void *)on_code, nullptr,
                JNI_STUB_PAGE, JNI_STUB_PAGE + 0x10000);
    uc_hook_add(g_uc, &h3,
                UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED |
                    UC_HOOK_MEM_FETCH_UNMAPPED,
                (void *)on_mem_fault, nullptr, 1, 0);

    /* run global constructors (DT_INIT + DT_INIT_ARRAY) exactly like
     * dlopen would, before JNI_OnLoad */
    const std::vector<uint32_t> &ctors = engine_init_routines();
    diag_log("[diag] running %u engine constructors", (unsigned)ctors.size());
    for (uint32_t fn : ctors) {
        uint32_t rva = fn - ENG_BASE;
        uint32_t args[2] = {0, 0};
        jni_call_engine_r0(g_uc, rva, 0, args, 2);
    }
    diag_log("[diag] constructors done");

    /* run the engine's own JNI_OnLoad */
    for (uint32_t i = 0; i < ENGINE_JNI_COUNT; i++) {
        if (strcmp(kEngineJniExports[i].name, "JNI_OnLoad") == 0) {
            diag_log("[diag] running engine JNI_OnLoad at rva 0x%x",
                     kEngineJniExports[i].rva);
            uint32_t r = jni_call_engine_r0(g_uc, kEngineJniExports[i].rva,
                                            JAVA_VM_OBJ, nullptr, 0);
            diag_log("[diag] engine JNI_OnLoad returned %u", r);
            break;
        }
    }
    LOGI("host initialized");
    diag_log("[diag] host initialized OK");
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
    if (!host_init(env)) {
        /* diag: solid red = host init failed */
        glClearColor(1.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        return 0;
    }
    /* diag: underlay so a dead engine shows magenta; the engine normally
       draws over it. */
    glClearColor(1.f, 0.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    uint32_t w[2] = {jni_handle_for(cls), (uint32_t)frame};
    uint32_t rva = engine_rva("Java_com_capcom_zombiecafeandroid_CapcomRenderer_render");
    if (!rva) {
        diag_log("[diag] render rva lookup FAILED");
        return 0;
    }
    uint32_t draws_before = g_diag_draws;
    uint32_t rv = jni_call_engine(g_uc, rva, w, 2);

    static int frame_count = 0;
    frame_count++;
    if ((frame_count % 60) == 1) {
        /* readback probe: what did the engine actually put on screen? */
        unsigned char px[4] = {0, 0, 0, 0};
        glReadPixels(1400, 630, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        GLenum err = glGetError();
        GLint vp[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, vp);
        GLint blend = glIsEnabled(GL_BLEND), scis = glIsEnabled(GL_SCISSOR_TEST),
             depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE),
             tex2d = glIsEnabled(GL_TEXTURE_2D);
        diag_log("[diag] f=%d draws=%u px=(%u,%u,%u,%u) glErr=%u vp=(%d,%d,%d,%d) blend=%d scis=%d depth=%d cull=%d tex=%d",
                 frame_count, g_diag_draws - draws_before, px[0], px[1], px[2],
                 px[3], err, vp[0], vp[1], vp[2], vp[3], blend, scis, depth,
                 cull, tex2d);
        diag_summary();
    }
    return (jint)rv;
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
    diag_log("[input] mouseDown x=%.1f y=%.1f t=%d", x, y, t);
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
    diag_log("[input] mouseUp x=%.1f y=%.1f t=%d", x, y, t);
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

/* ---- Mod-menu natives (ported from LemL0/Zombie-Cafe-Revival-2.0
 * ZombieCafeExtension.cpp - thanks LemL0!). The offsets are RVAs into the
 * original engine binary which we emulate, so the same layout applies:
 * the CapcomApp** global at 0x1daf80 -> app->field_4 = ZombieCafe*, whose
 * toxin/money/xp fields live at fixed offsets. These write emulated memory
 * directly; the engine picks the values up on its next tick. Only the
 * ModMenu build wires UI to these; the vanilla build never calls them. */
namespace {
constexpr uint32_t kAppHolderRva = 0x1daf80;
constexpr uint32_t kToxinOff = 0xb8;
constexpr uint32_t kMoneyOff = 0x1ac;
constexpr uint32_t kLevelXpOff = 0x1b4;
constexpr uint32_t kTotalXpOff = 0x1b8; /* float */
constexpr int32_t kMaxToxin = 999;
constexpr int32_t kMaxMoney = 999999;
constexpr int32_t kMaxXp = 999999;

uint32_t zombie_cafe_instance() {
    if (!g_uc) return 0;
    uint32_t app = 0, game = 0;
    if (!emu::read(g_uc, ENG_BASE + kAppHolderRva, &app, 4) || !app) return 0;
    if (!emu::read(g_uc, app + 4, &game, 4)) return 0;
    return game;
}
}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_DebugAddMoney(JNIEnv *env,
                                                                 jclass, jint amount) {
    if (!host_init(env)) return;
    uint32_t game = zombie_cafe_instance();
    if (!game) { diag_log("[mod] DebugAddMoney: game not ready"); return; }
    int32_t money = 0;
    emu::read(g_uc, game + kMoneyOff, &money, 4);
    int64_t next = (int64_t)money + amount;
    if (amount >= kMaxMoney || next > kMaxMoney) next = kMaxMoney;
    else if (next < 0) next = 0;
    money = (int32_t)next;
    emu::write(g_uc, game + kMoneyOff, &money, 4);
    diag_log("[mod] DebugAddMoney -> %d", money);
}

extern "C" JNIEXPORT void JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_DebugSetToxin(JNIEnv *env,
                                                                 jclass, jint amount) {
    if (!host_init(env)) return;
    uint32_t game = zombie_cafe_instance();
    if (!game) { diag_log("[mod] DebugSetToxin: game not ready"); return; }
    int32_t toxin = amount > kMaxToxin ? kMaxToxin : (amount < 0 ? 0 : amount);
    emu::write(g_uc, game + kToxinOff, &toxin, 4);
    diag_log("[mod] DebugSetToxin -> %d", toxin);
}

extern "C" JNIEXPORT void JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_DebugAddXp(JNIEnv *env,
                                                              jclass, jint amount) {
    if (!host_init(env)) return;
    uint32_t game = zombie_cafe_instance();
    if (!game) { diag_log("[mod] DebugAddXp: game not ready"); return; }
    int32_t level_xp = 0;
    float total_xp = 0.f;
    emu::read(g_uc, game + kLevelXpOff, &level_xp, 4);
    emu::read(g_uc, game + kTotalXpOff, &total_xp, 4);
    int64_t next = (int64_t)level_xp + amount;
    if (amount >= kMaxXp || next > kMaxXp) next = kMaxXp;
    else if (next < 0) next = 0;
    level_xp = (int32_t)next;
    total_xp += (float)amount;
    if (total_xp > (float)kMaxXp) total_xp = (float)kMaxXp;
    else if (total_xp < 0.f) total_xp = 0.f;
    emu::write(g_uc, game + kLevelXpOff, &level_xp, 4);
    emu::write(g_uc, game + kTotalXpOff, &total_xp, 4);
    diag_log("[mod] DebugAddXp -> %d / %.0f", level_xp, (double)total_xp);
}

extern "C" JNIEXPORT void JNICALL
Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_DebugSetXp(JNIEnv *env,
                                                              jclass, jint amount) {
    if (!host_init(env)) return;
    uint32_t game = zombie_cafe_instance();
    if (!game) { diag_log("[mod] DebugSetXp: game not ready"); return; }
    int32_t xp = amount > kMaxXp ? kMaxXp : (amount < 0 ? 0 : amount);
    float total = (float)xp;
    emu::write(g_uc, game + kLevelXpOff, &xp, 4);
    emu::write(g_uc, game + kTotalXpOff, &total, 4);
    diag_log("[mod] DebugSetXp -> %d", xp);
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
