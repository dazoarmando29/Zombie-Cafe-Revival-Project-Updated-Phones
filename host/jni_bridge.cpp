#include "jni_bridge.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <unicorn/unicorn.h>

#include "engine_memory.h"
#include "engine_loader.h"
#include "generated/engine_tables.h"
#include "stub_dispatch.h"

/* --------------------------------------------------------------------------
 * Java object handles. Real ART objects (64-bit pointers) are never exposed
 * to the emulated engine; instead the engine sees 32-bit handles that we map
 * back on the host side of JNI calls.
 * ------------------------------------------------------------------------ */
namespace {

std::vector<void *> g_handles(1, nullptr);  // handle 0 = null
std::unordered_map<void *, uint32_t> g_handle_of;

uint32_t handle_for(void *obj) {
    if (!obj) return 0;
    auto it = g_handle_of.find(obj);
    if (it != g_handle_of.end()) return it->second;
    g_handles.push_back(obj);
    uint32_t h = (uint32_t)g_handles.size() - 1;
    g_handle_of[obj] = h;
    return h;
}

void *handle_get(uint32_t h) {
    return h < g_handles.size() ? g_handles[h] : nullptr;
}

/* JNIEnv function slots: index = (offset in JNINativeInterface) / sizeof(fn).
 * The emulated vtable entry for each function is its trap address. */
constexpr uint32_t jni_trap(uint32_t index) {
    return JNI_STUB_PAGE + index * STUB_SLOT_SIZE;
}

/* Indices we implement (from the fixed JNINativeInterface layout). */
enum JniFn {
    kGetVersion = 4,
    kFindClass = 6,
    kGetMethodID = 33,
    kGetStaticMethodID = 113,
    kNewStringUTF = 167,
    kGetStringUTFLength = 168,
    kGetStringUTFChars = 169,
    kReleaseStringUTFChars = 170,
    kGetArrayLength = 171,
    kNewByteArray = 178,
    kGetByteArrayElements = 184,
    kReleaseByteArrayElements = 187,
    kExceptionCheck = 228,
    kExceptionClear = 17,
};

struct JniHandler {
    uint32_t index;
    const char *name;
    ImportHandler fn;
};

bool h_get_version(uc_engine *, uint32_t[], uint32_t *rv) {
    *rv = 0x00010006;  // JNI 1.6
    return true;
}

bool h_find_class(uc_engine *uc, uint32_t a[], uint32_t *rv) {
    /* a[1] = char* name in emulated memory */
    char name[256] = {0};
    uc_mem_read(uc, a[1], name, sizeof(name) - 1);
    fprintf(stderr, "[zcr-host] FindClass(%s) -> stub handle\n", name);
    *rv = handle_for(strdup(name));
    return true;
}

bool h_new_string_utf(uc_engine *uc, uint32_t a[], uint32_t *rv) {
    char buf[1024] = {0};
    uc_mem_read(uc, a[1], buf, sizeof(buf) - 1);
    *rv = handle_for(strdup(buf));
    return true;
}

bool h_get_string_utf_chars(uc_engine *uc, uint32_t a[], uint32_t *rv) {
    const char *s = (const char *)handle_get(a[1]);
    if (!s) { *rv = 0; return true; }
    uint32_t len = (uint32_t)strlen(s) + 1;
    /* copy into emulated heap; simple bump allocator would go here */
    static uint32_t bump = HEAP_BASE + HEAP_SIZE / 2;
    uc_mem_write(uc, bump, s, len);
    *rv = bump;
    bump += (len + 15u) & ~15u;
    return true;
}

bool h_noop_ok(uc_engine *, uint32_t[], uint32_t *rv) { *rv = 0; return true; }

const JniHandler kJniHandlers[] = {
    {kGetVersion, "GetVersion", h_get_version},
    {kFindClass, "FindClass", h_find_class},
    {kNewStringUTF, "NewStringUTF", h_new_string_utf},
    {kGetStringUTFChars, "GetStringUTFChars", h_get_string_utf_chars},
    {kGetStringUTFLength, "GetStringUTFLength", h_noop_ok},
    {kReleaseStringUTFChars, "ReleaseStringUTFChars", h_noop_ok},
    {kGetArrayLength, "GetArrayLength", h_noop_ok},
    {kNewByteArray, "NewByteArray", h_noop_ok},
    {kGetByteArrayElements, "GetByteArrayElements", h_noop_ok},
    {kReleaseByteArrayElements, "ReleaseByteArrayElements", h_noop_ok},
    {kGetMethodID, "GetMethodID", h_find_class},
    {kGetStaticMethodID, "GetStaticMethodID", h_find_class},
    {kExceptionCheck, "ExceptionCheck", h_noop_ok},
    {kExceptionClear, "ExceptionClear", h_noop_ok},
};

bool g_inited = false;

}  // namespace

bool jni_bridge_init(uc_engine *uc) {
    if (g_inited) return true;

    /* Fake JNIEnv object: first word = vtable pointer. */
    uint32_t vtable = JNI_STUB_PAGE;
    uint32_t env = JNI_ENV_OBJ;
    uc_mem_write(uc, env, &vtable, 4);
    uint32_t vm = JAVA_VM_OBJ;
    uc_mem_write(uc, JAVA_VM_OBJ, &vtable, 4);

    /* Register handlers onto the trap slots. */
    stubs_init();
    for (const auto &h : kJniHandlers) {
        char nm[64];
        snprintf(nm, sizeof(nm), "JNIEnv#%u", h.index);
        stubs_register(nm, h.fn);
    }
    g_inited = true;
    fprintf(stderr, "[zcr-host] JNI bridge up (env@0x%x vm@0x%x)\n", env, vm);
    return true;
}

uint32_t jni_call_engine(uc_engine *uc, uint32_t fn_rva,
                         uint32_t a0, uint32_t a1,
                         uint32_t a2, uint32_t a3) {
    uint32_t sp = STACK_BASE + STACK_SIZE - 256;
    uint32_t lr = EMU_RETURN_ADDR;
    uint32_t pc = ENG_ADDR(fn_rva);
    uint32_t env = JNI_ENV_OBJ;

    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    uc_reg_write(uc, UC_ARM_REG_LR, &lr);
    uc_reg_write(uc, UC_ARM_REG_R0, &env);
    uc_reg_write(uc, UC_ARM_REG_R1, &a0);
    uc_reg_write(uc, UC_ARM_REG_R2, &a1);
    uc_reg_write(uc, UC_ARM_REG_R3, &a2);
    uc_reg_write(uc, UC_ARM_REG_PC, &pc);
    (void)a3;

    uc_err err = uc_emu_start(uc, pc, EMU_RETURN_ADDR, 0, 0);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[zcr-host] emu error: %s\n", uc_strerror(err));
        return 0;
    }
    uint32_t rv = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &rv);
    return rv;
}
