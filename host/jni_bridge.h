/* JNI bridge: gives the emulated engine a fake JNIEnv/JavaVM whose function
 * pointers land on the JNI stub page, and exports the 35 Java_* entry points
 * that ART resolves from this (host) library. */
#pragma once

#include <jni.h>
#include <cstdint>

struct uc_struct;
typedef struct uc_struct uc_engine;

/* Create the emulated address space objects: JNIEnv at JNI_ENV_OBJ,
 * JavaVM at JAVA_VM_OBJ, each with a vtable on JNI_STUB_PAGE. */
bool jni_bridge_init(uc_engine *uc);

/* Run an emulated engine function to completion (used by every Java_*
 * entry point). Sets up r0-r3 + LR=EMU_RETURN_ADDR and emu-starts. */
uint32_t jni_call_engine(uc_engine *uc, uint32_t fn_rva,
                         uint32_t a0, uint32_t a1,
                         uint32_t a2, uint32_t a3);
