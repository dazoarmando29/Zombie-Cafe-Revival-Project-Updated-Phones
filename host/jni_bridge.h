/* JNI bridge: the emulated engine receives a fake JNIEnv/JavaVM whose
 * function pointers trap into the host, where we forward to the *real* ART
 * environment. All JNI references cross the boundary as 32-bit handles. */
#pragma once

#include <jni.h>
#include <cstdint>

struct uc_struct;
typedef struct uc_struct uc_engine;

bool jni_bridge_init(uc_engine *uc, JavaVM *vm);

/* Run an emulated engine export to completion.
 *   r0 = fake JNIEnv, r1.. = payload words (handles / ints / float bits).
 * Extra words beyond r1..r3 are placed on the emulated stack. */
uint32_t jni_call_engine(uc_engine *uc, uint32_t fn_rva, const uint32_t *words,
                         uint32_t nwords);

/* Same, but with an explicit r0 (e.g. JAVA_VM_OBJ for JNI_OnLoad). */
uint32_t jni_call_engine_r0(uc_engine *uc, uint32_t fn_rva, uint32_t r0,
                            const uint32_t *words, uint32_t nwords);

/* Same as jni_call_engine_r0, but the nested call runs on an explicit
 * emulated stack pointer instead of the fresh top-of-stack frame. Required
 * when calling engine functions from inside a code hook: the interrupted
 * engine frame is live at its current SP, so the nested frame must be placed
 * below it (typically hook_SP - 256). */
uint32_t jni_call_engine_r0_sp(uc_engine *uc, uint32_t fn_rva, uint32_t r0,
                               const uint32_t *words, uint32_t nwords,
                               uint32_t sp);

/* Handle <-> real JNI object mapping (used by the Java_* forwarders). */
uint32_t jni_handle_for(jobject obj);
jobject jni_handle_get(uint32_t h);

/* Current real env (attached thread). */
JNIEnv *jni_real_env();
