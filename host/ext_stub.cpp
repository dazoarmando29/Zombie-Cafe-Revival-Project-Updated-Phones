/* No-op replacement for libZombieCafeExtension on arm64.
 * The real patches are applied by zcr_host's engine_apply_zcr_patches(). */
#include <jni.h>
#include <android/log.h>

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *) {
    __android_log_print(ANDROID_LOG_INFO, "zcr-ext",
                        "stub extension loaded (patches applied by host)");
    return JNI_VERSION_1_6;
}
