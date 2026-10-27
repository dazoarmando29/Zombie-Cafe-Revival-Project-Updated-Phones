/* zcr-host: arm64 replacement for the original libZombieCafeAndroid.so.
 *
 * ART loads this library (same soname) and resolves the same Java_* symbols.
 * Each entry point forwards into the emulated 32-bit engine. */
#include <jni.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <unicorn/unicorn.h>

#include "engine_loader.h"
#include "engine_memory.h"
#include "jni_bridge.h"
#include "stub_dispatch.h"
#include "generated/engine_tables.h"

#define LOGI(...) \
    __android_log_print(ANDROID_LOG_INFO, "zcr-host", __VA_ARGS__)
#define LOGE(...) \
    __android_log_print(ANDROID_LOG_ERROR, "zcr-host", __VA_ARGS__)

namespace {

uc_engine *g_uc = nullptr;
EngineImage g_img;

/* Code hook: import stubs and the JNI trap page both trap here. */
bool on_code(uc_engine *uc, uint32_t address, uint32_t size, void *) {
    if (address == EMU_RETURN_ADDR) return false;  // stop emu_start

    if (address >= JNI_STUB_PAGE && address < JNI_STUB_PAGE + 0x10000) {
        uint32_t idx = (address - JNI_STUB_PAGE) / STUB_SLOT_SIZE;
        char nm[64];
        snprintf(nm, sizeof(nm), "JNIEnv#%u", idx);
        uint32_t regs[4];
        uc_reg_read(uc, UC_ARM_REG_R0, &regs[0]);
        uc_reg_read(uc, UC_ARM_REG_R1, &regs[1]);
        uc_reg_read(uc, UC_ARM_REG_R2, &regs[2]);
        uc_reg_read(uc, UC_ARM_REG_R3, &regs[3]);
        uint32_t lr = 0, rv = 0;
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        if (!stubs_dispatch_by_name(uc, nm, regs, &rv)) {
            LOGE("unimplemented JNIEnv fn #%u", idx);
        }
        uc_reg_write(uc, UC_ARM_REG_R0, &rv);
        uc_reg_write(uc, UC_ARM_REG_PC, &lr);
        return true;
    }

    if (address >= STUB_PAGE && address < STUB_PAGE + 0x10000) {
        stubs_dispatch(uc, address);
        return true;
    }
    return true;
}

bool load_engine_from_assets(JNIEnv *env) {
    /* Preferred: engine ELF shipped as a raw asset (assets/engine/game.so). */
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID curApp = env->GetStaticMethodID(
        activityThread, "currentApplication", "()Landroid/app/Application;");
    jobject app = env->CallStaticObjectMethod(activityThread, curApp);
    if (!app) return false;
    jclass ctxCls = env->FindClass("android/content/Context");
    jmethodID getAssets = env->GetMethodID(
        ctxCls, "getAssets", "()Landroid/content/res/AssetManager;");
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
    g_img.entry_rva = 0;
    return true;
}

bool host_init(JNIEnv *env) {
    if (g_uc) return true;

    if (!load_engine_from_assets(env)) {
        LOGE("failed to load engine image");
        return false;
    }

    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &g_uc);
    if (err != UC_ERR_OK) {
        LOGE("uc_open: %s", uc_strerror(err));
        return false;
    }

    /* Stack + heap regions the loader does not create. */
    uc_mem_map(g_uc, STACK_BASE, STACK_SIZE, UC_PROT_ALL);
    uc_mem_map(g_uc, HEAP_BASE, HEAP_SIZE, UC_PROT_ALL);
    uc_mem_map(g_uc, STUB_PAGE, 0x10000, UC_PROT_ALL);
    uc_mem_map(g_uc, JNI_STUB_PAGE, 0x10000, UC_PROT_ALL);
    uc_mem_map(g_uc, JNI_ENV_OBJ, 0x1000, UC_PROT_ALL);

    if (!engine_map(g_uc, g_img)) return false;
    engine_apply_zcr_patches(g_uc);
    stubs_init();
    jni_bridge_init(g_uc);

    uc_hook hh;
    uc_hook_add(g_uc, &hh, UC_HOOK_CODE, (void *)on_code, nullptr, 1, 0);

    /* Run the engine's JNI_OnLoad (registers natives, starts the game). */
    for (uint32_t i = 0; i < ENGINE_JNI_COUNT; i++) {
        if (strcmp(kEngineJniExports[i].name, "JNI_OnLoad") == 0) {
            uint32_t vm = JAVA_VM_OBJ;
            jni_call_engine(g_uc, kEngineJniExports[i].rva, vm, 0, 0, 0);
            break;
        }
    }
    LOGI("host initialized");
    return true;
}

/* Forward any Java_* call to the engine export of the same name. */
#define FORWARD(sym)                                                   \
    extern "C" JNIEXPORT jint JNICALL sym(JNIEnv *env, jobject thiz) { \
        (void)thiz;                                                    \
        if (!host_init(env)) return 0;                                 \
        uint32_t rva = 0;                                              \
        for (uint32_t i = 0; i < ENGINE_JNI_COUNT; i++)                \
            if (strcmp(kEngineJniExports[i].name, #sym) == 0) {        \
                rva = kEngineJniExports[i].rva;                        \
                break;                                                 \
            }                                                          \
        return (jint)jni_call_engine(g_uc, rva, 0, 0, 0, 0);           \
    }

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JNIEnv *env = nullptr;
    vm->GetEnv((void **)&env, JNI_VERSION_1_4);
    host_init(env);
    return JNI_VERSION_1_4;
}

/* ---- the engine's 35 JNI entry points (subset shown; generated next) ---- */
FORWARD(Java_com_capcom_zombiecafeandroid_CapcomRenderer_render)
FORWARD(Java_com_capcom_zombiecafeandroid_CapcomRenderer_CreateGame)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseDown)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseUp)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_mouseMove)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_pause)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_InitMethodCalls)
FORWARD(Java_com_capcom_zombiecafeandroid_SmurfsRenderer_resize)
FORWARD(Java_com_capcom_zombiecafeandroid_SmurfsRenderer_setTexture)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_deviceShaken)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_HandleBackButton)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_DialogCallBack)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_ClearDialogFlag)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_CheckDoneLoading)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_CheckIfInHelpScreen)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_OnFriendScreen)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_StartNotifications)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_WatchedTrailer)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_PurchaseAndroidToxin)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_setDeviceModel)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_setVanityString)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_updateAccelerometer)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_ReloadTask_reloadTextures)
FORWARD(Java_com_capcom_zombiecafeandroid_SoundManager_setEnabled)
FORWARD(Java_com_capcom_zombiecafeandroid_SmurfsGLSurfaceView_DebugGame)
FORWARD(Java_com_capcom_zombiecafeandroid_URLManager_LoadedBannerTextureCallback)
FORWARD(Java_com_capcom_zombiecafeandroid_URLManager_NewRequestCallback)
FORWARD(Java_com_capcom_zombiecafeandroid_URLManager_NewRequestServerCallback)
FORWARD(Java_com_capcom_zombiecafeandroid_NetworkTask_NewRequestServerCallback)
FORWARD(Java_com_capcom_zombiecafeandroid_NetworkTaskPost_NewRequestServerCallback)
FORWARD(Java_com_capcom_zombiecafeandroid_CapcomFacebook_setFBInfo)
FORWARD(Java_com_capcom_zombiecafeandroid_CapcomFacebook_sendFriendInfo)
FORWARD(Java_com_capcom_zombiecafeandroid_CapcomFacebook_onFacebook)
FORWARD(Java_com_capcom_zombiecafeandroid_FacebookCallback)
FORWARD(Java_com_capcom_zombiecafeandroid_ZombieCafeAndroid_FacebookCallback)
