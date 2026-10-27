/* JNI bridge: the emulated engine receives a fake JNIEnv/JavaVM whose
 * function pointers trap into the host, where we forward to the *real* ART
 * environment. All JNI references cross the boundary as 32-bit handles. */
#include "jni_bridge.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <android/log.h>
#include <unicorn/unicorn.h>

#include "engine_memory.h"
#include "emu_mem.h"
#include "stub_dispatch.h"
#include "diag.h"

#define TAG "zcr-jni"
#define JIDX(field) ((uint32_t)(offsetof(JNINativeInterface, field) / sizeof(void *)))

namespace {

JavaVM *g_vm = nullptr;
JNIEnv *g_env = nullptr;

/* ---- handle table: 32-bit handle <-> real JNI reference ---- */
std::mutex g_mu;
std::vector<void *> g_handles(1, nullptr);
std::unordered_map<void *, uint32_t> g_handle_of;

struct MethodEntry {
    jmethodID id;
    std::string sig;
    bool is_static;
};
std::unordered_map<uint32_t, MethodEntry> g_methods;
uint32_t g_method_next = 0x70000000;

uint32_t handle_for(void *obj) {
    if (!obj) return 0;
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_handle_of.find(obj);
    if (it != g_handle_of.end()) return it->second;
    g_handles.push_back(obj);
    uint32_t h = (uint32_t)g_handles.size() - 1;
    g_handle_of[obj] = h;
    return h;
}
void *handle_get(uint32_t h) {
    std::lock_guard<std::mutex> lk(g_mu);
    return h < g_handles.size() ? g_handles[h] : nullptr;
}

const MethodEntry *method_of(uint32_t h) {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_methods.find(h);
    return it == g_methods.end() ? nullptr : &it->second;
}

uint32_t method_handle(jmethodID m, const std::string &sig, bool is_static) {
    std::lock_guard<std::mutex> lk(g_mu);
    uint32_t h = g_method_next++;
    g_methods[h] = {m, sig, is_static};
    return h;
}

/* ---- argument sources ---- */
struct WordSrc {
    virtual uint32_t next() = 0;
    virtual ~WordSrc() = default;
};

/* inline words: r0..r3 then stack (varargs ABI) */
struct RegSrc : WordSrc {
    emu::Args a;
    unsigned i = 3;
    explicit RegSrc(const emu::Args &args) : a(args) {}
    uint32_t next() override { return a.arg(i++); }
};

/* words at a memory address (va_list on ARM32 is char*) */
struct MemSrc : WordSrc {
    uc_engine *uc;
    uint32_t addr;
    MemSrc(uc_engine *u, uint32_t ad) : uc(u), addr(ad) {}
    uint32_t next() override {
        uint32_t v = 0;
        emu::read(uc, addr, &v, 4);
        addr += 4;
        return v;
    }
};

void marshal(WordSrc &src, const std::string &sig, std::vector<jvalue> *out) {
    size_t p = sig.find('(');
    if (p == std::string::npos) return;
    p++;
    while (p < sig.size() && sig[p] != ')') {
        jvalue v;
        memset(&v, 0, sizeof(v));
        switch (sig[p]) {
            case 'Z': v.z = (jboolean)src.next(); break;
            case 'B': v.b = (jbyte)src.next(); break;
            case 'C': v.c = (jchar)src.next(); break;
            case 'S': v.s = (jshort)src.next(); break;
            case 'I': v.i = (jint)src.next(); break;
            case 'J': {
                uint32_t lo = src.next(), hi = src.next();
                v.j = ((jlong)hi << 32) | lo;
                break;
            }
            case 'F': {
                uint32_t u = src.next();
                memcpy(&v.f, &u, 4);
                break;
            }
            case 'D': {
                uint32_t lo = src.next(), hi = src.next();
                uint64_t u = ((uint64_t)hi << 32) | lo;
                memcpy(&v.d, &u, 8);
                break;
            }
            case 'L': {
                v.l = (jobject)handle_get(src.next());
                size_t sc = sig.find(';', p);
                p = sc == std::string::npos ? sig.size() : sc;
                break;
            }
            case '[': {
                v.l = (jobject)handle_get(src.next());
                p++;
                while (p < sig.size() && sig[p] == '[') p++;
                if (p < sig.size() && sig[p] == 'L') {
                    size_t sc = sig.find(';', p);
                    p = sc == std::string::npos ? sig.size() : sc;
                }
                break;
            }
            default: break;
        }
        out->push_back(v);
        p++;
    }
}

/* jvalue array in emulated memory (8 bytes each on 32-bit) */
void marshal_a(uc_engine *uc, uint32_t jvalues, const std::string &sig,
               std::vector<jvalue> *out) {
    size_t p = sig.find('(');
    if (p == std::string::npos) return;
    p++;
    uint32_t off = 0;
    while (p < sig.size() && sig[p] != ')') {
        jvalue v;
        memset(&v, 0, sizeof(v));
        uint8_t raw[8] = {0};
        emu::read(uc, jvalues + off, raw, 8);
        uint32_t w0 = raw[0] | (raw[1] << 8) | (raw[2] << 16) | (raw[3] << 24);
        switch (sig[p]) {
            case 'Z': v.z = (jboolean)w0; break;
            case 'B': v.b = (jbyte)w0; break;
            case 'C': v.c = (jchar)w0; break;
            case 'S': v.s = (jshort)w0; break;
            case 'I': v.i = (jint)w0; break;
            case 'J': {
                uint32_t w1 = raw[4] | (raw[5] << 8) | (raw[6] << 16) | (raw[7] << 24);
                v.j = ((jlong)w1 << 32) | w0;
                break;
            }
            case 'F': memcpy(&v.f, &w0, 4); break;
            case 'D': memcpy(&v.d, raw, 8); break;
            case 'L': {
                v.l = (jobject)handle_get(w0);
                size_t sc = sig.find(';', p);
                p = sc == std::string::npos ? sig.size() : sc;
                break;
            }
            case '[': {
                v.l = (jobject)handle_get(w0);
                p++;
                while (p < sig.size() && sig[p] == '[') p++;
                if (p < sig.size() && sig[p] == 'L') {
                    size_t sc = sig.find(';', p);
                    p = sc == std::string::npos ? sig.size() : sc;
                }
                break;
            }
            default: break;
        }
        out->push_back(v);
        off += 8;
        p++;
    }
}

/* ---- call dispatch ---- */
enum class Ret { kVoid, kBool, kInt, kFloat, kLong, kObject };

void do_call(uc_engine *uc, const emu::Args &a, WordSrc &src, Ret ret,
             uint32_t *rv, bool is_static) {
    uint32_t obj_or_cls = a.r[1], mid_h = a.r[2];
    const MethodEntry *m = method_of(mid_h);
    if (!m) {
        *rv = 0;
        return;
    }
    std::vector<jvalue> args;
    marshal(src, m->sig, &args);
    jobject obj = (jobject)handle_get(obj_or_cls);
    if (!obj) {
        *rv = 0;
        return;
    }
    switch (ret) {
        case Ret::kVoid:
            if (is_static) g_env->CallStaticVoidMethodA((jclass)obj, m->id, args.data());
            else g_env->CallVoidMethodA(obj, m->id, args.data());
            *rv = 0;
            break;
        case Ret::kBool:
            *rv = is_static ? (uint32_t)g_env->CallStaticBooleanMethodA((jclass)obj, m->id, args.data())
                            : (uint32_t)g_env->CallBooleanMethodA(obj, m->id, args.data());
            break;
        case Ret::kInt:
            *rv = is_static ? (uint32_t)g_env->CallStaticIntMethodA((jclass)obj, m->id, args.data())
                            : (uint32_t)g_env->CallIntMethodA(obj, m->id, args.data());
            break;
        case Ret::kFloat: {
            jfloat f = is_static ? g_env->CallStaticFloatMethodA((jclass)obj, m->id, args.data())
                                 : g_env->CallFloatMethodA(obj, m->id, args.data());
            memcpy(rv, &f, 4);
            break;
        }
        case Ret::kLong: {
            jlong l = is_static ? g_env->CallStaticLongMethodA((jclass)obj, m->id, args.data())
                                : g_env->CallLongMethodA(obj, m->id, args.data());
            *rv = (uint32_t)l;
            uint32_t hi = (uint32_t)((uint64_t)l >> 32);
            uc_reg_write(uc, UC_ARM_REG_R1, &hi);
            break;
        }
        case Ret::kObject: {
            jobject o = is_static ? g_env->CallStaticObjectMethodA((jclass)obj, m->id, args.data())
                                  : g_env->CallObjectMethodA(obj, m->id, args.data());
            *rv = handle_for(o);
            break;
        }
    }
}

void do_call_a(uc_engine *uc, const emu::Args &a, Ret ret, uint32_t *rv,
               bool is_static) {
    uint32_t obj_or_cls = a.r[1], mid_h = a.r[2], jvals = a.r[3];
    const MethodEntry *m = method_of(mid_h);
    if (!m) {
        *rv = 0;
        return;
    }
    std::vector<jvalue> args;
    marshal_a(uc, jvals, m->sig, &args);
    jobject obj = (jobject)handle_get(obj_or_cls);
    if (!obj) {
        *rv = 0;
        return;
    }
    switch (ret) {
        case Ret::kVoid:
            if (is_static) g_env->CallStaticVoidMethodA((jclass)obj, m->id, args.data());
            else g_env->CallVoidMethodA(obj, m->id, args.data());
            *rv = 0;
            break;
        case Ret::kBool:
            *rv = is_static ? (uint32_t)g_env->CallStaticBooleanMethodA((jclass)obj, m->id, args.data())
                            : (uint32_t)g_env->CallBooleanMethodA(obj, m->id, args.data());
            break;
        case Ret::kInt:
            *rv = is_static ? (uint32_t)g_env->CallStaticIntMethodA((jclass)obj, m->id, args.data())
                            : (uint32_t)g_env->CallIntMethodA(obj, m->id, args.data());
            break;
        case Ret::kFloat: {
            jfloat f = is_static ? g_env->CallStaticFloatMethodA((jclass)obj, m->id, args.data())
                                 : g_env->CallFloatMethodA(obj, m->id, args.data());
            memcpy(rv, &f, 4);
            break;
        }
        case Ret::kLong: {
            jlong l = is_static ? g_env->CallStaticLongMethodA((jclass)obj, m->id, args.data())
                                : g_env->CallLongMethodA(obj, m->id, args.data());
            *rv = (uint32_t)l;
            uint32_t hi = (uint32_t)((uint64_t)l >> 32);
            uc_reg_write(uc, UC_ARM_REG_R1, &hi);
            break;
        }
        case Ret::kObject: {
            jobject o = is_static ? g_env->CallStaticObjectMethodA((jclass)obj, m->id, args.data())
                                  : g_env->CallObjectMethodA(obj, m->id, args.data());
            *rv = handle_for(o);
            break;
        }
    }
}

/* call with the receiver in r1 but args starting at r3 (varargs) */
#define DEF_CALL_FAMILY(name, retk, isst)                                    \
    void f_##name(uc_engine *uc, const emu::Args &a, uint32_t *rv) {         \
        RegSrc src(a);                                                       \
        do_call(uc, a, src, retk, rv, isst);                                 \
    }                                                                        \
    void f_##name##_a(uc_engine *uc, const emu::Args &a, uint32_t *rv) {     \
        do_call_a(uc, a, retk, rv, isst);                                    \
    }                                                                        \
    void f_##name##_v(uc_engine *uc, const emu::Args &a, uint32_t *rv) {     \
        MemSrc src(uc, a.r[3]);                                              \
        do_call(uc, a, src, retk, rv, isst);                                 \
    }

DEF_CALL_FAMILY(call_static_void, Ret::kVoid, true)
DEF_CALL_FAMILY(call_static_bool, Ret::kBool, true)
DEF_CALL_FAMILY(call_static_int, Ret::kInt, true)
DEF_CALL_FAMILY(call_static_float, Ret::kFloat, true)
DEF_CALL_FAMILY(call_static_long, Ret::kLong, true)
DEF_CALL_FAMILY(call_static_object, Ret::kObject, true)
DEF_CALL_FAMILY(call_void, Ret::kVoid, false)
DEF_CALL_FAMILY(call_bool, Ret::kBool, false)
DEF_CALL_FAMILY(call_int, Ret::kInt, false)
DEF_CALL_FAMILY(call_float, Ret::kFloat, false)
DEF_CALL_FAMILY(call_long, Ret::kLong, false)
DEF_CALL_FAMILY(call_object, Ret::kObject, false)

/* ---- method id lookups ---- */
void f_get_static_method_id(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string name, sig;
    emu::read_str(uc, a.r[2], &name);
    emu::read_str(uc, a.r[3], &sig);
    jclass cls = (jclass)handle_get(a.r[1]);
    if (!cls) {
        *rv = 0;
        return;
    }
    jmethodID m = g_env->GetStaticMethodID(cls, name.c_str(), sig.c_str());
    if (!m) {
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        *rv = 0;
        return;
    }
    *rv = method_handle(m, sig, true);
}
void f_get_method_id(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string name, sig;
    emu::read_str(uc, a.r[2], &name);
    emu::read_str(uc, a.r[3], &sig);
    jclass cls = (jclass)handle_get(a.r[1]);
    if (!cls) {
        *rv = 0;
        return;
    }
    jmethodID m = g_env->GetMethodID(cls, name.c_str(), sig.c_str());
    if (!m) {
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        *rv = 0;
        return;
    }
    *rv = method_handle(m, sig, false);
}

/* ---- class / string / array ---- */
void f_find_class(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string name;
    emu::read_str(uc, a.r[1], &name);
    jclass c = g_env->FindClass(name.c_str());
    if (!c && g_env->ExceptionCheck()) g_env->ExceptionClear();
    *rv = handle_for(c);
}
void f_new_string_utf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s;
    emu::read_str(uc, a.r[1], &s);
    *rv = handle_for(g_env->NewStringUTF(s.c_str()));
}
void f_get_string_utf_chars(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jstring s = (jstring)handle_get(a.r[1]);
    if (!s) {
        *rv = 0;
        return;
    }
    const char *p = g_env->GetStringUTFChars(s, nullptr);
    *rv = p ? emu::intern_string(uc, p) : 0;
    if (p) g_env->ReleaseStringUTFChars(s, p);
}
void f_get_string_utf_length(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jstring s = (jstring)handle_get(a.r[1]);
    *rv = s ? (uint32_t)g_env->GetStringUTFLength(s) : 0;
}
void f_new_byte_array(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = handle_for(g_env->NewByteArray((jsize)a.r[1]));
}
void f_release_string_utf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = 0;
}
void f_get_array_length(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jarray arr = (jarray)handle_get(a.r[1]);
    *rv = arr ? (uint32_t)g_env->GetArrayLength(arr) : 0;
}

/* Generic Get<prim>ArrayElements: copy into emulated heap, return emu ptr. */
void get_elements(uc_engine *uc, jarray arr, jsize n, jsize esz,
                  void (*region)(JNIEnv *, jarray, jsize, jsize, void *),
                  uint32_t *rv) {
    if (!arr) {
        *rv = 0;
        return;
    }
    uint32_t p = emu::heap_alloc(uc, (size_t)n * esz ? (size_t)n * esz : 1);
    if (n) {
        std::vector<uint8_t> buf((size_t)n * esz);
        region(g_env, arr, 0, n, buf.data());
        emu::write(uc, p, buf.data(), buf.size());
    }
    *rv = p;
}

/* Generic Release<prim>ArrayElements: write back unless JNI_ABORT. */
void release_elements(uc_engine *uc, jarray arr, uint32_t ptr, jint mode,
                      jsize n, jsize esz,
                      void (*setregion)(JNIEnv *, jarray, jsize, jsize, void *)) {
    if (arr && mode != JNI_ABORT && n) {
        std::vector<uint8_t> buf((size_t)n * esz);
        emu::read(uc, ptr, buf.data(), buf.size());
        setregion(g_env, arr, 0, n, buf.data());
    }
    emu::heap_free(uc, ptr);
}

void f_get_byte_array_elements(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jbyteArray arr = (jbyteArray)handle_get(a.r[1]);
    jsize n = arr ? g_env->GetArrayLength(arr) : 0;
    get_elements(uc, arr, n, 1,
                 [](JNIEnv *e, jarray x, jsize s, jsize c, void *p) {
                     e->GetByteArrayRegion((jbyteArray)x, s, c, (jbyte *)p);
                 },
                 rv);
}
void f_release_byte_array_elements(uc_engine *uc, const emu::Args &a,
                                   uint32_t *rv) {
    jbyteArray arr = (jbyteArray)handle_get(a.r[1]);
    jsize n = arr ? g_env->GetArrayLength(arr) : 0;
    release_elements(uc, arr, a.r[2], (jint)a.r[3], n, 1,
                     [](JNIEnv *e, jarray x, jsize s, jsize c, void *p) {
                         e->SetByteArrayRegion((jbyteArray)x, s, c, (jbyte *)p);
                     });
    *rv = 0;
}
void f_get_int_array_elements(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jintArray arr = (jintArray)handle_get(a.r[1]);
    jsize n = arr ? g_env->GetArrayLength(arr) : 0;
    get_elements(uc, arr, n, 4,
                 [](JNIEnv *e, jarray x, jsize s, jsize c, void *p) {
                     e->GetIntArrayRegion((jintArray)x, s, c, (jint *)p);
                 },
                 rv);
}
void f_release_int_array_elements(uc_engine *uc, const emu::Args &a,
                                  uint32_t *rv) {
    jintArray arr = (jintArray)handle_get(a.r[1]);
    jsize n = arr ? g_env->GetArrayLength(arr) : 0;
    release_elements(uc, arr, a.r[2], (jint)a.r[3], n, 4,
                     [](JNIEnv *e, jarray x, jsize s, jsize c, void *p) {
                         e->SetIntArrayRegion((jintArray)x, s, c, (jint *)p);
                     });
    *rv = 0;
}
void f_get_float_array_elements(uc_engine *uc, const emu::Args &a,
                                uint32_t *rv) {
    jfloatArray arr = (jfloatArray)handle_get(a.r[1]);
    jsize n = arr ? g_env->GetArrayLength(arr) : 0;
    get_elements(uc, arr, n, 4,
                 [](JNIEnv *e, jarray x, jsize s, jsize c, void *p) {
                     e->GetFloatArrayRegion((jfloatArray)x, s, c, (jfloat *)p);
                 },
                 rv);
}
void f_release_float_array_elements(uc_engine *uc, const emu::Args &a,
                                    uint32_t *rv) {
    jfloatArray arr = (jfloatArray)handle_get(a.r[1]);
    jsize n = arr ? g_env->GetArrayLength(arr) : 0;
    release_elements(uc, arr, a.r[2], (jint)a.r[3], n, 4,
                     [](JNIEnv *e, jarray x, jsize s, jsize c, void *p) {
                         e->SetFloatArrayRegion((jfloatArray)x, s, c, (jfloat *)p);
                     });
    *rv = 0;
}
void f_new_int_array(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = handle_for(g_env->NewIntArray((jsize)a.r[1]));
}
void f_new_float_array(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = handle_for(g_env->NewFloatArray((jsize)a.r[1]));
}
void f_get_int_array_region(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jintArray arr = (jintArray)handle_get(a.r[1]);
    if (arr) {
        std::vector<jint> buf(a.r[3]);
        if (!buf.empty())
            g_env->GetIntArrayRegion(arr, (jsize)a.r[2], (jsize)a.r[3], buf.data());
        emu::write(uc, a.arg(4), buf.data(), buf.size() * 4);
    }
    *rv = 0;
}
void f_set_int_array_region(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jintArray arr = (jintArray)handle_get(a.r[1]);
    if (arr) {
        std::vector<jint> buf(a.r[3]);
        if (!buf.empty()) {
            emu::read(uc, a.arg(4), buf.data(), buf.size() * 4);
            g_env->SetIntArrayRegion(arr, (jsize)a.r[2], (jsize)a.r[3], buf.data());
        }
    }
    *rv = 0;
}
void f_get_float_array_region(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jfloatArray arr = (jfloatArray)handle_get(a.r[1]);
    if (arr) {
        std::vector<jfloat> buf(a.r[3]);
        if (!buf.empty())
            g_env->GetFloatArrayRegion(arr, (jsize)a.r[2], (jsize)a.r[3], buf.data());
        emu::write(uc, a.arg(4), buf.data(), buf.size() * 4);
    }
    *rv = 0;
}
void f_set_float_array_region(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jfloatArray arr = (jfloatArray)handle_get(a.r[1]);
    if (arr) {
        std::vector<jfloat> buf(a.r[3]);
        if (!buf.empty()) {
            emu::read(uc, a.arg(4), buf.data(), buf.size() * 4);
            g_env->SetFloatArrayRegion(arr, (jsize)a.r[2], (jsize)a.r[3], buf.data());
        }
    }
    *rv = 0;
}
void f_get_byte_array_region(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jbyteArray arr = (jbyteArray)handle_get(a.r[1]);
    if (arr) {
        std::vector<jbyte> buf(a.r[3]);
        if (!buf.empty())
            g_env->GetByteArrayRegion(arr, (jsize)a.r[2], (jsize)a.r[3], buf.data());
        emu::write(uc, a.arg(4), buf.data(), buf.size());
    }
    *rv = 0;
}
void f_set_byte_array_region(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jbyteArray arr = (jbyteArray)handle_get(a.r[1]);
    if (arr) {
        std::vector<jbyte> buf(a.r[3]);
        if (!buf.empty()) {
            emu::read(uc, a.arg(4), buf.data(), buf.size());
            g_env->SetByteArrayRegion(arr, (jsize)a.r[2], (jsize)a.r[3],
                                      buf.data());
        }
    }
    *rv = 0;
}

/* ---- refs / misc ---- */
void f_new_global_ref(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = handle_for(g_env->NewGlobalRef((jobject)handle_get(a.r[1])));
}
void f_delete_global_ref(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = 0;
}
void f_delete_local_ref(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = 0;
}
void f_get_object_class(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    jobject o = (jobject)handle_get(a.r[1]);
    *rv = o ? handle_for(g_env->GetObjectClass(o)) : 0;
}
void f_exception_check(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = g_env->ExceptionCheck() ? 1 : 0;
}
void f_exception_clear(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_env->ExceptionClear();
    *rv = 0;
}
void f_exception_describe(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_env->ExceptionDescribe();
    *rv = 0;
}
void f_get_version(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = 0x00010006;
}
void f_get_java_vm(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    /* the engine keeps this as a JavaVM* in emulated memory -> must be the
       emulated fake VM object, not a host handle */
    uint32_t vm = JAVA_VM_OBJ;
    emu::write(uc, a.r[1], &vm, 4);
    *rv = 0;
}
void f_noop(uc_engine *uc, const emu::Args &a, uint32_t *rv) { *rv = 0; }

/* ---- JavaVM (JNIInvokeInterface) ----
 * indices: 3 DestroyJavaVM, 4 AttachCurrentThread, 5 DetachCurrentThread,
 *          6 GetEnv, 7 AttachCurrentThreadAsDaemon */
void f_vm_get_env(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    /* GetEnv(JavaVM*, void** env, jint version) -> hand back the fake env */
    uint32_t env = JNI_ENV_OBJ;
    if (a.r[1]) emu::write(uc, a.r[1], &env, 4);
    *rv = 0; /* JNI_OK */
}
void f_vm_attach(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    /* AttachCurrentThread(JavaVM*, void** env, void* args) */
    uint32_t env = JNI_ENV_OBJ;
    if (a.r[1]) emu::write(uc, a.r[1], &env, 4);
    *rv = 0;
}

/* ---- vtable registration ---- */
struct JniFn {
    uint32_t index;
    const char *name;
    ImportHandler fn;
};

const JniFn kJniFns[] = {
    {JIDX(GetVersion), "GetVersion", f_get_version},
    {JIDX(FindClass), "FindClass", f_find_class},
    {JIDX(GetStaticMethodID), "GetStaticMethodID", f_get_static_method_id},
    {JIDX(GetMethodID), "GetMethodID", f_get_method_id},

    {JIDX(CallStaticVoidMethod), "CallStaticVoidMethod", f_call_static_void},
    {JIDX(CallStaticVoidMethodV), "CallStaticVoidMethodV", f_call_static_void_v},
    {JIDX(CallStaticVoidMethodA), "CallStaticVoidMethodA", f_call_static_void_a},
    {JIDX(CallStaticBooleanMethod), "CallStaticBooleanMethod", f_call_static_bool},
    {JIDX(CallStaticBooleanMethodV), "CallStaticBooleanMethodV", f_call_static_bool_v},
    {JIDX(CallStaticBooleanMethodA), "CallStaticBooleanMethodA", f_call_static_bool_a},
    {JIDX(CallStaticIntMethod), "CallStaticIntMethod", f_call_static_int},
    {JIDX(CallStaticIntMethodV), "CallStaticIntMethodV", f_call_static_int_v},
    {JIDX(CallStaticIntMethodA), "CallStaticIntMethodA", f_call_static_int_a},
    {JIDX(CallStaticFloatMethod), "CallStaticFloatMethod", f_call_static_float},
    {JIDX(CallStaticFloatMethodV), "CallStaticFloatMethodV", f_call_static_float_v},
    {JIDX(CallStaticFloatMethodA), "CallStaticFloatMethodA", f_call_static_float_a},
    {JIDX(CallStaticLongMethod), "CallStaticLongMethod", f_call_static_long},
    {JIDX(CallStaticLongMethodV), "CallStaticLongMethodV", f_call_static_long_v},
    {JIDX(CallStaticLongMethodA), "CallStaticLongMethodA", f_call_static_long_a},
    {JIDX(CallStaticObjectMethod), "CallStaticObjectMethod", f_call_static_object},
    {JIDX(CallStaticObjectMethodV), "CallStaticObjectMethodV", f_call_static_object_v},
    {JIDX(CallStaticObjectMethodA), "CallStaticObjectMethodA", f_call_static_object_a},

    {JIDX(CallVoidMethod), "CallVoidMethod", f_call_void},
    {JIDX(CallVoidMethodV), "CallVoidMethodV", f_call_void_v},
    {JIDX(CallVoidMethodA), "CallVoidMethodA", f_call_void_a},
    {JIDX(CallBooleanMethod), "CallBooleanMethod", f_call_bool},
    {JIDX(CallBooleanMethodA), "CallBooleanMethodA", f_call_bool_a},
    {JIDX(CallIntMethod), "CallIntMethod", f_call_int},
    {JIDX(CallIntMethodA), "CallIntMethodA", f_call_int_a},
    {JIDX(CallFloatMethod), "CallFloatMethod", f_call_float},
    {JIDX(CallFloatMethodA), "CallFloatMethodA", f_call_float_a},
    {JIDX(CallLongMethod), "CallLongMethod", f_call_long},
    {JIDX(CallLongMethodA), "CallLongMethodA", f_call_long_a},
    {JIDX(CallObjectMethod), "CallObjectMethod", f_call_object},
    {JIDX(CallObjectMethodA), "CallObjectMethodA", f_call_object_a},

    {JIDX(NewStringUTF), "NewStringUTF", f_new_string_utf},
    {JIDX(GetStringUTFChars), "GetStringUTFChars", f_get_string_utf_chars},
    {JIDX(GetStringUTFLength), "GetStringUTFLength", f_get_string_utf_length},
    {JIDX(ReleaseStringUTFChars), "ReleaseStringUTFChars", f_release_string_utf},
    {JIDX(GetArrayLength), "GetArrayLength", f_get_array_length},
    {JIDX(NewByteArray), "NewByteArray", f_new_byte_array},
    {JIDX(NewIntArray), "NewIntArray", f_new_int_array},
    {JIDX(NewFloatArray), "NewFloatArray", f_new_float_array},
    {JIDX(GetByteArrayElements), "GetByteArrayElements", f_get_byte_array_elements},
    {JIDX(GetIntArrayElements), "GetIntArrayElements", f_get_int_array_elements},
    {JIDX(GetFloatArrayElements), "GetFloatArrayElements", f_get_float_array_elements},
    {JIDX(ReleaseByteArrayElements), "ReleaseByteArrayElements", f_release_byte_array_elements},
    {JIDX(ReleaseIntArrayElements), "ReleaseIntArrayElements", f_release_int_array_elements},
    {JIDX(ReleaseFloatArrayElements), "ReleaseFloatArrayElements", f_release_float_array_elements},
    {JIDX(GetByteArrayRegion), "GetByteArrayRegion", f_get_byte_array_region},
    {JIDX(SetByteArrayRegion), "SetByteArrayRegion", f_set_byte_array_region},
    {JIDX(GetIntArrayRegion), "GetIntArrayRegion", f_get_int_array_region},
    {JIDX(SetIntArrayRegion), "SetIntArrayRegion", f_set_int_array_region},
    {JIDX(GetFloatArrayRegion), "GetFloatArrayRegion", f_get_float_array_region},
    {JIDX(SetFloatArrayRegion), "SetFloatArrayRegion", f_set_float_array_region},
    {JIDX(NewGlobalRef), "NewGlobalRef", f_new_global_ref},
    {JIDX(DeleteGlobalRef), "DeleteGlobalRef", f_delete_global_ref},
    {JIDX(DeleteLocalRef), "DeleteLocalRef", f_delete_local_ref},
    {JIDX(GetObjectClass), "GetObjectClass", f_get_object_class},
    {JIDX(ExceptionCheck), "ExceptionCheck", f_exception_check},
    {JIDX(ExceptionClear), "ExceptionClear", f_exception_clear},
    {JIDX(ExceptionDescribe), "ExceptionDescribe", f_exception_describe},
    {JIDX(GetJavaVM), "GetJavaVM", f_get_java_vm},

    /* JavaVM vtable (registered under JVM# slots 512+i) */
    {JAVA_VM_SLOT_BASE + 6, "JVM#6:GetEnv", f_vm_get_env},
    {JAVA_VM_SLOT_BASE + 4, "JVM#4:AttachCurrentThread", f_vm_attach},
    {JAVA_VM_SLOT_BASE + 5, "JVM#5:DetachCurrentThread", f_noop},
    {JAVA_VM_SLOT_BASE + 3, "JVM#3:DestroyJavaVM", f_noop},
    {JAVA_VM_SLOT_BASE + 7, "JVM#7:AttachAsDaemon", f_vm_attach},
};

bool g_inited = false;

}  // namespace

JNIEnv *jni_real_env() { return g_env; }
uint32_t jni_handle_for(jobject obj) { return handle_for(obj); }
jobject jni_handle_get(uint32_t h) { return (jobject)handle_get(h); }

bool jni_bridge_init(uc_engine *uc, JavaVM *vm) {
    if (g_inited) return true;
    g_vm = vm;
    vm->GetEnv((void **)&g_env, JNI_VERSION_1_6);

    /* Fake JNIEnv object: first word = JNIEnv vtable. */
    uint32_t env_vtable = JNI_STUB_PAGE;
    emu::write(uc, JNI_ENV_OBJ, &env_vtable, 4);

    /* Fake JavaVM object: its vtable is the JNIInvokeInterface layout
     * (reserved x3, DestroyJavaVM, AttachCurrentThread, DetachCurrentThread,
     * GetEnv, AttachCurrentThreadAsDaemon) - distinct from JNIEnv's. */
    uint32_t vm_vtable = JAVA_VM_VTABLE;
    emu::write(uc, JAVA_VM_OBJ, &vm_vtable, 4);

    /* Fill the emulated JNIEnv vtable: entry i -> trap address for slot i. */
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t trap = JNI_STUB_PAGE + i * STUB_SLOT_SIZE;
        emu::write(uc, JNI_STUB_PAGE + i * 4, &trap, 4);
    }
    /* JavaVM vtable: entry i -> trap at slot (512 + i) */
    for (uint32_t i = 0; i < 16; i++) {
        uint32_t trap = JNI_STUB_PAGE + (JAVA_VM_SLOT_BASE + i) * STUB_SLOT_SIZE;
        emu::write(uc, JAVA_VM_VTABLE + i * 4, &trap, 4);
    }

    for (const auto &f : kJniFns) {
        char nm[64];
        snprintf(nm, sizeof(nm), "JNIEnv#%u", f.index);
        stubs_register(nm, f.fn);
    }
    g_inited = true;
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "JNI bridge ready (env@0x%x vm@0x%x)", JNI_ENV_OBJ,
                        JAVA_VM_OBJ);
    return true;
}

uint32_t jni_call_engine(uc_engine *uc, uint32_t fn_rva, const uint32_t *words,
                         uint32_t nwords) {
    return jni_call_engine_r0(uc, fn_rva, JNI_ENV_OBJ, words, nwords);
}

uint32_t jni_call_engine_r0(uc_engine *uc, uint32_t fn_rva, uint32_t r0,
                            const uint32_t *words, uint32_t nwords) {
    /* render thread + UI thread both drive the emulator; the engine's own
       Java callbacks can re-enter on the same thread -> recursive mutex */
    static std::recursive_mutex call_mu;
    std::lock_guard<std::recursive_mutex> lk(call_mu);

    uint32_t sp = STACK_BASE + STACK_SIZE - 1024;
    uint32_t lr = EMU_RETURN_ADDR;
    uint32_t pc = ENG_ADDR(fn_rva);

    uint32_t regs[4] = {r0, 0, 0, 0};
    for (uint32_t i = 0; i < nwords && i < 3; i++) regs[i + 1] = words[i];
    uint32_t stack_words[16] = {0};
    uint32_t nstack = 0;
    for (uint32_t i = 3; i < nwords && nstack < 16; i++)
        stack_words[nstack++] = words[i];
    if (nstack) emu::write(uc, sp, stack_words, nstack * 4);

    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    uc_reg_write(uc, UC_ARM_REG_LR, &lr);
    uc_reg_write(uc, UC_ARM_REG_R0, &regs[0]);
    uc_reg_write(uc, UC_ARM_REG_R1, &regs[1]);
    uc_reg_write(uc, UC_ARM_REG_R2, &regs[2]);
    uc_reg_write(uc, UC_ARM_REG_R3, &regs[3]);
    uc_reg_write(uc, UC_ARM_REG_PC, &pc);

    for (;;) {
        uc_err err = uc_emu_start(uc, pc, EMU_RETURN_ADDR, 0, 0);
        uc_reg_read(uc, UC_ARM_REG_PC, &pc);
        if (pc == EMU_RETURN_ADDR) break;
        if (err != UC_ERR_OK) {
            diag_note_emu_error("jni_call_engine", uc_strerror(err));
            break;
        }
        ImportHandler fn = nullptr;
        emu::Args args;
        stubs_take_pending(&fn, &args);
        if (fn) {
            uint32_t retval = 0, lr2 = 0;
            uc_reg_read(uc, UC_ARM_REG_LR, &lr2);
            fn(uc, args, &retval);
            uc_reg_write(uc, UC_ARM_REG_R0, &retval);
            emu_set_pc(uc, lr2);
            pc = lr2;   /* bit0 selects Thumb for uc_emu_start */
            continue;
        }
        break;
    }
    uint32_t rv = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &rv);
    return rv;
}
