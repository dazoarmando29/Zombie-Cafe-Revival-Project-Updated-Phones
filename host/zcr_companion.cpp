/* Recipe mastery + achievements companion layer. See zcr_companion.h. */
#include "zcr_companion.h"

#include <android/asset_manager.h>
#include <android/log.h>
#include <jni.h>
#include <unicorn/unicorn.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "companion_state.h"
#include "diag.h"
#include "emu_mem.h"
#include "engine_memory.h"
#include "jni_bridge.h"

namespace zcr_companion {
namespace {

/* ---- verified engine RVAs / fields (docs/ASSET_EXPANSION_GUIDE.md §5) ---- */
constexpr uint32_t RVA_COOKED = 0x82048; /* DishToCounterTask::completeObjective */
constexpr uint32_t OFF_TASK_FOOD = 0x3c;  /* DishToCounterTask+0x3c = FoodStack* */
constexpr uint32_t OFF_FS_RECIPE = 0x0;   /* FoodStack+0 = recipe row (byte) */
constexpr uint32_t OFF_FS_VARIANT = 0x8;  /* FoodStack+8 = variant (byte) */
constexpr uint32_t RVA_SPEND = 0x11fc94;  /* ZombieCafe::spendToxin(int) */
constexpr uint32_t RVA_ADD = 0x122134;    /* ZombieCafe::addToxin(int,bool) */
constexpr uint32_t RVA_APP_HOLDER = 0x1daf80; /* CapcomApp** global */
constexpr uint32_t OFF_APP_GAME = 0x4;    /* CapcomApp+4 = ZombieCafe* */
constexpr uint32_t OFF_TOXIN = 0xb8;      /* ZombieCafe+0xb8 = toxin (spendToxin disasm) */
constexpr int32_t kMaxToxin = 999;

constexpr const char *kFileName = "zcr_companion.json";
constexpr const char *kAchAsset = "data/zcr_achievements.json";
constexpr const char *kLockAsset = "data/zcr_registry.lock.json";
constexpr const char *kLockFoodFile = "foodData.bin.mid.json";
constexpr int64_t kFlushMinGap = 10; /* seconds, unless forced */

uc_engine *g_uc = nullptr;
AAssetManager *g_amgr = nullptr;
std::mutex g_mu;
zcr_comp::State g_state;
zcr_comp::Defs g_defs;
std::map<int32_t, std::string> g_row_ids;
std::string g_path;        /* <filesDir>/zcr_companion.json */
jobject g_context = nullptr; /* global ref to the Application */
int64_t g_last_flush = 0;
int64_t g_last_export = 0;
bool g_dirty = false;
bool g_export_pending = false;
bool g_ready = false;

void logi(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, "zcr-comp", "%s", buf);
    diag_log("[zcr-comp] %s", buf);
}

/* ---- asset access ---- */

bool read_asset(const char *path, std::string *out) {
    if (!g_amgr) return false;
    AAsset *a = AAssetManager_open(g_amgr, path, AASSET_MODE_STREAMING);
    if (!a) return false;
    char tmp[4096];
    out->clear();
    for (;;) {
        int n = AAsset_read(a, tmp, sizeof(tmp));
        if (n <= 0) break;
        out->append(tmp, (size_t)n);
    }
    AAsset_close(a);
    return !out->empty();
}

/* ---- app context / files dir (mirrors resolve_log_dir) ---- */

bool resolve_app(JNIEnv *env) {
    jclass atCls = env->FindClass("android/app/ActivityThread");
    if (!atCls) return false;
    jmethodID curApp = env->GetStaticMethodID(atCls, "currentApplication",
                                              "()Landroid/app/Application;");
    if (!curApp) return false;
    jobject app = env->CallStaticObjectMethod(atCls, curApp);
    if (!app) return false;
    g_context = env->NewGlobalRef(app);

    jclass ctxCls = env->FindClass("android/content/Context");
    jmethodID getFiles =
        ctxCls ? env->GetMethodID(ctxCls, "getFilesDir", "()Ljava/io/File;")
               : nullptr;
    if (!getFiles) return false;
    jobject dir = env->CallObjectMethod(app, getFiles);
    if (!dir) return false;
    jclass fileCls = env->FindClass("java/io/File");
    jmethodID getPath =
        fileCls ? env->GetMethodID(fileCls, "getPath", "()Ljava/lang/String;")
                : nullptr;
    if (!getPath) return false;
    jstring path = (jstring)env->CallObjectMethod(dir, getPath);
    if (!path) return false;
    const char *cpath = env->GetStringUTFChars(path, nullptr);
    if (!cpath) return false;
    g_path = std::string(cpath) + "/" + kFileName;
    env->ReleaseStringUTFChars(path, cpath);
    return !g_path.empty();
}

/* SaveGuard lives in patches/smali (both build variants) and is already the
 * tested save-export path; the companion file rides the same pipeline. */
void saveguard_call(bool restore) {
    JNIEnv *env = jni_real_env();
    if (!env || !g_context) return;
    jclass sg = env->FindClass("com/capcom/zombiecafeandroid/SaveGuard");
    if (!sg) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        logi("SaveGuard class not found");
        return;
    }
    jstring name = env->NewStringUTF(kFileName);
    if (restore) {
        jmethodID m = env->GetStaticMethodID(
            sg, "restoreInternal", "(Landroid/content/Context;Ljava/lang/String;)Z");
        if (m) {
            jboolean ok = env->CallStaticBooleanMethod(sg, m, g_context, name);
            logi("restoreInternal -> %d", (int)ok);
        }
    } else {
        jmethodID m = env->GetStaticMethodID(
            sg, "exportSave", "(Landroid/content/Context;Ljava/lang/String;)V");
        if (m) env->CallStaticVoidMethod(sg, m, g_context, name);
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        logi("SaveGuard call failed");
    }
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(sg);
}

/* ---- persistence ---- */

bool write_file(const std::string &path, const std::string &data) {
    std::string tmp = path + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    size_t n = fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    if (n != data.size()) {
        remove(tmp.c_str());
        return false;
    }
    remove(path.c_str());
    return rename(tmp.c_str(), path.c_str()) == 0;
}

bool read_file(const std::string &path, std::string *out) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    char tmp[4096];
    out->clear();
    for (;;) {
        size_t n = fread(tmp, 1, sizeof(tmp), f);
        if (n == 0) break;
        out->append(tmp, n);
    }
    fclose(f);
    return true;
}

/* SaveGuard.exportSave throttles per file (10s) on the Java side; calling
 * it while throttled silently drops the copy, which can leave the export
 * one write behind after a process death. Mirror the throttle here and
 * keep a pending flag so the render-tick flush retries it. */
void export_guarded(int64_t now) {
    if (now - g_last_export < kFlushMinGap) {
        g_export_pending = true;
        return;
    }
    saveguard_call(false);
    g_last_export = now;
    g_export_pending = false;
}

void flush_locked(bool force) {
    if (!g_ready) return;
    if (!force && !g_dirty) {
        /* still retry a throttled export when its window opens */
        if (g_export_pending) export_guarded((int64_t)time(nullptr));
        return;
    }
    int64_t now = (int64_t)time(nullptr);
    if (!force && now - g_last_flush < kFlushMinGap) return;
    g_state.updated_ts = now;
    std::string json = zcr_comp::serialize(g_state);
    if (!write_file(g_path, json)) {
        logi("flush FAILED to %s", g_path.c_str());
        return;
    }
    g_last_flush = now;
    g_dirty = false;
    export_guarded(now);
}

/* ---- engine memory helpers ---- */

bool plausible_ptr(uint32_t p) {
    return p >= HEAP_BASE && p < HEAP_BASE + HEAP_SIZE && (p & 3u) == 0;
}

uint32_t game_instance() {
    uint32_t app = 0, game = 0;
    if (!g_uc) return 0;
    if (!emu::read(g_uc, ENG_BASE + RVA_APP_HOLDER, &app, 4) || !app) return 0;
    if (!emu::read(g_uc, app + OFF_APP_GAME, &game, 4)) return 0;
    return game;
}

void grant_toxin(int32_t reward) {
    uint32_t game = game_instance();
    if (!game) {
        logi("reward grant skipped: no game instance");
        return;
    }
    int32_t cur = 0;
    emu::read(g_uc, game + OFF_TOXIN, &cur, 4);
    int64_t next = (int64_t)cur + reward;
    if (next > kMaxToxin) next = kMaxToxin;
    if (next < 0) next = 0;
    int32_t v = (int32_t)next;
    emu::write(g_uc, game + OFF_TOXIN, &v, 4);
    logi("reward granted: +%d toxin -> %d", (int)reward, (int)v);
}

/* Reward pipeline: persist-then-grant. The claim latch is flushed BEFORE
 * the toxin is added, so a crash between the two loses the reward (safe)
 * instead of allowing a re-claim (exploit). */
void handle_unlocked(const std::vector<std::string> &ids, int64_t now) {
    for (const std::string &id : ids) {
        int32_t reward = zcr_comp::achievement_reward(g_defs, id);
        zcr_comp::mark_claimed(g_state, id, now);
        g_dirty = true;
        flush_locked(true);
        if (reward > 0) grant_toxin(reward);
        logi("achievement unlocked: %s (reward %d toxin)", id.c_str(),
             (int)reward);
    }
}

std::string recipe_key_for(int32_t row) {
    auto it = g_row_ids.find(row);
    if (it != g_row_ids.end()) return it->second;
    return zcr_comp::recipe_key("", row);
}

/* ---- engine hooks (read-only: never mutate engine state/registers) ---- */

/* DishToCounterTask::completeObjective - the engine calls
 * ObjectiveSystem::cookFood(recipeRow) from exactly this spot, so the
 * recipe byte at [task+0x3c]->+0 is the engine's own "dish cooked"
 * accounting. Matches engine semantics (dishes that burn unclaimed are
 * not counted; aborted carries are - same as the in-game objectives). */
void on_cooked(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    uint32_t task = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &task);
    uint32_t fs = 0;
    if (!emu::read(uc, task + OFF_TASK_FOOD, &fs, 4) || !plausible_ptr(fs)) {
        logi("cook event with no dish (task=0x%x fs=0x%x)", task, fs);
        return;
    }
    uint8_t rowb = 0, varb = 0;
    if (!emu::read(uc, fs + OFF_FS_RECIPE, &rowb, 1)) return;
    emu::read(uc, fs + OFF_FS_VARIANT, &varb, 1);
    if (rowb >= 250) return; /* byte ids are recipe rows; 250+ is nonsense */
    std::string key = recipe_key_for(rowb);
    int64_t now = (int64_t)time(nullptr);
    auto unlocked = zcr_comp::on_dish_cooked(g_state, g_defs, key, rowb, now);
    g_dirty = true;
    logi("cooked: row=%u variant=%u key=%s total=%lld", (unsigned)rowb,
         (unsigned)varb, key.c_str(), (long long)g_state.total_cooked);
    handle_unlocked(unlocked, now);
    flush_locked(false);
}

void on_spend(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    uint32_t amount = 0;
    uc_reg_read(uc, UC_ARM_REG_R1, &amount);
    if (amount == 0 || amount > 100000u) return;
    int64_t now = (int64_t)time(nullptr);
    auto unlocked =
        zcr_comp::on_toxin_spent(g_state, g_defs, (int64_t)amount, now);
    g_dirty = true;
    logi("toxin spent: %u (total %lld)", (unsigned)amount,
         (long long)g_state.toxin_spent);
    handle_unlocked(unlocked, now);
    flush_locked(false);
}

void on_add(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    uint32_t amount = 0, flag = 0;
    uc_reg_read(uc, UC_ARM_REG_R1, &amount);
    uc_reg_read(uc, UC_ARM_REG_R2, &flag);
    if (amount == 0 || amount > 100000u) return;
    if (flag != 0) {
        /* purchase flow (GiveBoughtToxin passes flag=1): not gameplay
         * earnings - tracked only in the log for transparency */
        logi("toxin purchased: %u (not counted as earned)", (unsigned)amount);
        return;
    }
    int64_t now = (int64_t)time(nullptr);
    auto unlocked =
        zcr_comp::on_toxin_earned(g_state, g_defs, (int64_t)amount, now);
    g_dirty = true;
    logi("toxin earned: %u (total %lld)", (unsigned)amount,
         (long long)g_state.toxin_earned);
    handle_unlocked(unlocked, now);
    flush_locked(false);
}

}  // namespace

bool init(uc_engine *uc, void *asset_mgr) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_uc = uc;
    g_amgr = (AAssetManager *)asset_mgr;

    JNIEnv *env = jni_real_env();
    if (!env || !resolve_app(env)) {
        logi("no app context; companion disabled");
        return false;
    }

    /* achievement definitions (data driven) */
    {
        std::string json;
        std::string err;
        if (read_asset(kAchAsset, &json)) {
            if (!zcr_comp::parse_defs(json, &g_defs, &err)) {
                logi("achievement defs invalid: %s", err.c_str());
                g_defs = zcr_comp::Defs();
            }
        } else {
            logi("achievement defs asset missing (%s)", kAchAsset);
        }
        logi("defs: %zu achievements, mastery thresholds=%zu",
             g_defs.achievements.size(), g_defs.mastery_levels.size());
    }

    /* registry row -> zcrId map for recipes */
    {
        std::string json;
        std::string err;
        if (read_asset(kLockAsset, &json)) {
            if (!zcr_comp::parse_registry_rows(json, kLockFoodFile, &g_row_ids,
                                               &err))
                logi("registry lock parse failed: %s", err.c_str());
        } else {
            logi("registry lock asset missing (%s)", kLockAsset);
        }
        logi("registry: %zu recipe rows mapped to ZcrIds", g_row_ids.size());
    }

    /* restore (no-op when the file already exists) then load */
    saveguard_call(true);
    {
        std::string json;
        std::string err;
        if (read_file(g_path, &json)) {
            if (!zcr_comp::parse_state(json, &g_state, &err)) {
                logi("companion save corrupt (%s); preserving + starting fresh",
                     err.c_str());
                std::string bak = g_path + ".corrupt";
                remove(bak.c_str());
                rename(g_path.c_str(), bak.c_str());
                g_state = zcr_comp::State();
            }
        }
        if (g_state.created_ts == 0) {
            g_state.created_ts = (int64_t)time(nullptr);
            g_dirty = true;
        }
        /* achievements latched unlocked-but-unclaimed at load complete their
         * reward claim now (crash between persist and grant path) */
        std::vector<std::string> pending;
        for (const auto &kv : g_state.achievements)
            if (kv.second.unlocked && !kv.second.claimed) pending.push_back(kv.first);
        logi("state loaded: cooked=%lld recipes=%zu toxin spent=%lld earned=%lld "
             "ach=%zu pending-claims=%zu",
             (long long)g_state.total_cooked, g_state.recipes.size(),
             (long long)g_state.toxin_spent, (long long)g_state.toxin_earned,
             g_state.achievements.size(), pending.size());
        g_ready = true;
        if (!pending.empty()) {
            int64_t now = (int64_t)time(nullptr);
            handle_unlocked(pending, now);
        }
    }

    /* self-test: run the pipeline on a scratch copy (device evidence that
     * state updates + serialization work in situ; nothing is persisted) */
    {
        zcr_comp::State probe = g_state;
        int64_t now = (int64_t)time(nullptr);
        zcr_comp::on_dish_cooked(probe, g_defs, "row:0", 0, now);
        std::string json = zcr_comp::serialize(probe);
        zcr_comp::State back;
        std::string err;
        bool ok = zcr_comp::parse_state(json, &back, &err);
        logi("selftest: pipeline %s (probe total=%lld roundtrip=%d)",
             ok ? "ok" : "FAILED", (long long)probe.total_cooked, (int)ok);
    }

    uc_hook h1 = 0, h2 = 0, h3 = 0;
    uc_err e1 = uc_hook_add(uc, &h1, UC_HOOK_CODE, (void *)on_cooked, nullptr,
                            ENG_ADDR(RVA_COOKED), ENG_ADDR(RVA_COOKED));
    uc_err e2 = uc_hook_add(uc, &h2, UC_HOOK_CODE, (void *)on_spend, nullptr,
                            ENG_ADDR(RVA_SPEND), ENG_ADDR(RVA_SPEND));
    uc_err e3 = uc_hook_add(uc, &h3, UC_HOOK_CODE, (void *)on_add, nullptr,
                            ENG_ADDR(RVA_ADD), ENG_ADDR(RVA_ADD));
    if (e1 != UC_ERR_OK || e2 != UC_ERR_OK || e3 != UC_ERR_OK) {
        logi("hook add failed: %s / %s / %s", uc_strerror(e1), uc_strerror(e2),
             uc_strerror(e3));
        return false;
    }
    logi("hooks installed (cooked@0x%x spend@0x%x add@0x%x)", RVA_COOKED,
         RVA_SPEND, RVA_ADD);
    flush_locked(false);
    return true;
}

void flush(bool force) {
    std::lock_guard<std::mutex> lk(g_mu);
    flush_locked(force);
}

}  // namespace zcr_companion
