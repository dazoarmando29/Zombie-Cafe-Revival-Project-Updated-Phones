/* Recipe mastery + achievements companion layer. See zcr_companion.h. */
#include "zcr_companion.h"

#include <android/asset_manager.h>
#include <android/log.h>
#include <jni.h>
#include <unicorn/unicorn.h>

#include <algorithm>
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
/* recipe favorites (docs/ASSET_EXPANSION_GUIDE.md §9.5): the star flag is
 * Recipe+0x1e and resets every launch in the original game */
constexpr uint32_t RVA_UPDATE_FAV = 0xfb8e8; /* RecipeBook::updateFavorite(bool) */
constexpr uint32_t RVA_LOAD_BOOK = 0xffa1c;  /* RecipeBook::loadCookBook(bool) */
constexpr uint32_t RVA_LIST_GROW = 0xfb7c0;  /* int-list grow(list*, newCap) */
constexpr uint32_t OFF_BOOK_SELROW = 0x214;  /* RecipeBook selected recipe row */
constexpr uint32_t OFF_BOOK_LIST = 0x64;     /* favorites widget ptr */
constexpr uint32_t OFF_LIST_CAP = 0x38;      /* widget list: capacity */
constexpr uint32_t OFF_LIST_COUNT = 0x3c;    /* widget list: count (entries at 1..count) */
constexpr uint32_t OFF_LIST_ARRAY = 0x40;    /* widget list: int* rows */
constexpr uint32_t OFF_GAME_RECIPES = 0x128; /* ZombieCafe recipe table */
constexpr uint32_t RECIPE_STRIDE = 0x20;
constexpr uint32_t OFF_RECIPE_FAV = 0x1e;    /* favorite flag (byte) */
constexpr int kMaxFavorites = 14;            /* engine list cap (0xd+1) */
constexpr uint32_t RVA_APP_HOLDER = 0x1daf80; /* CapcomApp** global */
constexpr uint32_t OFF_APP_GAME = 0x4;    /* CapcomApp+4 = ZombieCafe* */
constexpr uint32_t OFF_TOXIN = 0xb8;      /* ZombieCafe+0xb8 = toxin (spendToxin disasm) */
constexpr int32_t kMaxToxin = 999;

/* ---- toxin-spend confirmation gate (docs/ASSET_EXPANSION_GUIDE.md §9.6/9.7)
 * The 20-site spendToxin audit found every player-facing spend behind the
 * engine's ConfirmDialog except GameStateZukan::buyCharacter (zookdex
 * character purchase: one-tap spend after a balance check). Gate it with the
 * engine's own dialog (ConfirmDialog* at zukan+0xb0) and re-enter the
 * purchase through a tiny Thumb code cave that restores the saved purchase
 * index - the spend itself stays 100% engine code. The dialog's callback
 * fires only on the affirmative button (ConfirmDialog::tick 0x794f6 path). */
constexpr uint32_t RVA_ZUKAN_BUY = 0x1358b4;   /* GameStateZukan::buyCharacter */
constexpr uint32_t OFF_ZUKAN_DLG = 0xb0;       /* GameStateZukan+0xb0 = ConfirmDialog* */
constexpr uint32_t RVA_STR_CTOR = 0x141f1c;    /* String::String(char const*) */
constexpr uint32_t RVA_STR_DTOR = 0x142470;    /* String::~String() */
constexpr uint32_t RVA_CONFIRM_SHOW = 0x78e58; /* ConfirmDialog::show(msg,yes,no,cb,ctx) */
constexpr uint32_t CAVE_PAGE = 0xF3000000u;    /* Thumb re-entry cave (mapped RWX) */
constexpr uint32_t CAVE_INDEX_OFF = 0x0c;      /* cave literal: saved purchase index */
constexpr uint32_t CAVE_RET_OFF = 8u;          /* blx site+4 = LR of cave re-entry */

/* ---- QoL: auto-cook / auto-serve / serve guard (docs/QOL_RE_NOTES) ----
 * Verified engine ABI (2026-11-02 disasm):
 *   GameStateCafe::cook(GameStateCafe*, Stove*, row, variant, CapcomDate,
 *                       String, String) @0x9d534 - CapcomDate = 8B by value
 *   then 2 String handles; cost check inside (Recipe cost @0xfa350,
 *   money at ZombieCafe+0x1ac); silent return when unaffordable.
 *   ZombieCafe+0x8 = active GameStateCafe*, GameStateCafe+0x14 = Cafe*.
 *   Cafe::createDistanceSortedFurnitureList(cafe, type, 0) @0x65e7c returns
 *   a list object ([list+8] = head; node+0 = item, node+8 = next); type 1 =
 *   stoves (matches advanceStoves), 2 = counters, 4 = seats.
 *   Stove+0x60 = FoodStack* (idle iff 0).
 *   CafeObject+0x2e = in-use flag: seat claimed (FindSeatTask) or food
 *   placed (ServeTask); EatTask clears it when waiting/eaten. ServeTask
 *   never checks it before placing - the overwrite bug.
 *   javaSendNotification(type, msg, extra, delaySec) @0x17e474 -> Java
 *   AlarmManager notification at now+delaySec. */
constexpr uint32_t RVA_COOK = 0x9d534;         /* GameStateCafe::cook */
constexpr uint32_t RVA_STOVE_LIST = 0x65f20;   /* Cafe::createDistanceSortedStoveList */
constexpr uint32_t RVA_RECIPE_COST = 0xfa350;  /* Recipe cost (Recipe*, variant) */
constexpr uint32_t RVA_DATE_NOW = 0x199cc8;    /* CapcomDate now (out* 8B) */
constexpr uint32_t RVA_STR_FROM_INT = 0x141ba8;/* String::String(int) (empty for 0) */
constexpr uint32_t RVA_SEND_NOTIF = 0x17e474;  /* javaSendNotification */
constexpr uint32_t RVA_FREE = 0x138e88;        /* operator delete */
constexpr uint32_t OFF_GAME_STATE = 0x8;       /* ZombieCafe+0x8 = GameStateCafe* */
constexpr uint32_t OFF_STATE_CAFE = 0x14;      /* GameStateCafe+0x14 = Cafe* */
constexpr uint32_t OFF_STATE_GAME = 0xc;       /* GameStateCafe+0xc = ZombieCafe* */
constexpr uint32_t OFF_GAME_MONEY = 0x1ac;     /* ZombieCafe+0x1ac = coins */
constexpr uint32_t OFF_STOVE_FOOD = 0x60;      /* Stove+0x60 = FoodStack* (may go stale) */
constexpr uint32_t OFF_RECIPE_COOKTYPE = 0x1a; /* Recipe+0x1a = appliance filter */
constexpr uint32_t OFF_TASK_TARGET = 0x28;     /* ServeTask+0x28 = target */
constexpr uint32_t OFF_OBJ_BUSY = 0x2e;        /* CafeObject+0x2e = in-use */
constexpr uint32_t RVA_SERVE_PLACE = 0x10d386; /* ServeTask::tick placement block */
constexpr uint32_t RVA_SERVE_ABORT = 0x10d408; /* ServeTask::tick return-dish path */
constexpr uint32_t RVA_CAN_FOLLOW = 0x9b35c;   /* characterCanFollow gate */
constexpr uint32_t OFF_STATE_SELSTOVE = 0x50;  /* GameStateCafe+0x50 = UI stove sel */
constexpr uint32_t OFF_STATE_LOOPSTOVE = 0x54; /* GameStateCafe+0x54 = chef-loop stove */
constexpr uint32_t OFF_STATE_SELCHAR = 0x58;   /* GameStateCafe+0x58 = selected char */
constexpr uint32_t OFF_STATE_CHARS = 0x20;     /* GameStateCafe+0x20 = char count */
constexpr uint32_t OFF_STATE_CHARARR = 0x24;   /* GameStateCafe+0x24 = char array */
constexpr uint32_t RVA_COOK_TASK = 0x80d9c;    /* CookTask(char, stove, cafe) ctor */
constexpr uint32_t RVA_DISH_TASK = 0x81f8c;    /* DishToCounterTask(char, cafe, stove) */
constexpr uint32_t RVA_CLEAN_TASK = 0x789c8;   /* CleanupTask(char, cafe, furniture) */
constexpr uint32_t RVA_ASSIGN_TASK = 0x6b1f0;  /* CharacterInstance::assignTask */
constexpr uint32_t RVA_IS_DONE = 0x77ecc;      /* Stove::isDoneCooking */
constexpr uint32_t RVA_IS_BURNED = 0x77f04;    /* Stove::isBurned */
constexpr uint32_t RVA_NEW = 0x138eac;         /* operator new */
constexpr uint32_t OFF_CHAR_DATA = 0x94;       /* CharacterInstance+0x94 = CharacterData* */
constexpr uint32_t OFF_CHARDATA_IDLE = 0x48;   /* CharacterData+0x48: <=0 = idle */
constexpr uint32_t RVA_SELECT_CHAR = 0x9b3dc;  /* GameStateCafe::select(char) */
constexpr int kBulkVariant = 5;                /* BULK add-on (cost x2, double servings) */
constexpr int64_t kAutoCookGapSec = 5;         /* driver throttle */
constexpr int64_t kNotifGapSec = 1800;         /* unaffordable notif throttle */

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

/* ---- toxin spend flow attribution ----
 * spendToxin's LR = return address = caller BL site + 4. The table below is
 * the complete BL-site map from the 2026-11-02 raw-byte BL scan (docs/
 * ASSET_EXPANSION_GUIDE.md §9.6). Audit verdict per site: 19 of 20 sit in
 * ConfirmDialog-gated flows (GOT-wired yes-callbacks / *Confirmed handlers);
 * zukan_buy's common path was the single one-tap gap and is now gated by the
 * on_zukan_buy hook below; CharacterDetails::heal is dead code (no xrefs). */
struct SpendSite {
    uint32_t site; /* BL instruction RVA */
    const char *flow;
};
const SpendSite kSpendSites[] = {
    {0x6fcea, "character_heal"},     /* CharacterCommandBar::healConfirmed */
    {0x72552, "character_heal"},     /* CharacterDetails::heal */
    {0x7eaa2, "cook_finish"},        /* CookCommandBar::finishDish */
    {0x9bc4e, "cafe_buy"},           /* GameStateCafe::buyConfirmed */
    {0xa2cae, "infect_purchase"},    /* GameStateCafe::completeInfectPurchase */
    {0xa56dc, "revive"},             /* GameStateCafe::onReviveConfirmed */
    {0xa756c, "character_heal"},     /* GameStateCafe::onHealConfirmed */
    {0xaaf18, "store_purchase"},     /* GameStateCafe::completeStorePurchase */
    {0xb1364, "chef_buy"},           /* GameStateChooseChef::buyConfirmed */
    {0xba7d6, "friend_heal"},        /* GameStateFriendCafe::onHealConfirmed */
    {0xc9126, "map_reopen"},         /* GameStateMap::onReOpen */
    {0xcf3c4, "raid_heal"},          /* GameStateRaid::onHealConfirmed */
    {0xe55b4, "objective_bribe"},    /* ObjectiveDialog::onBribeConfirmed */
    {0xf2a2e, "order_finish"},       /* CookingOrder::finish */
    {0x10129e, "recipe_unlock"},     /* RecipeBook::onUnlockButtonPressed */
    {0x1015a0, "cookbook_buy"},      /* RecipeBook::onConfirmbuyCookBookbyToxin */
    {0x11614c, "tombstone_recharge"},/* TombstoneCommandBar::rechargeTombstone */
    {0x13162e, "locker_buy"},        /* GameStateMeetLocker::buyConfirmed */
    {0x135a56, "zukan_buy"},         /* GameStateZukan::buyCharacter */
    {0x135b86, "zukan_buy"},         /* GameStateZukan::buyConfirmed */
};

std::string flow_for_caller(uint32_t lr) {
    uint32_t site = (lr & ~1u) - 4u; /* Thumb return addr -> BL site */
    for (const SpendSite &s : kSpendSites)
        if (s.site == site) return s.flow;
    char buf[32];
    snprintf(buf, sizeof(buf), "other_0x%x", (unsigned)site);
    return buf;
}

void on_spend(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    uint32_t amount = 0, lr = 0;
    uc_reg_read(uc, UC_ARM_REG_R1, &amount);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    if (amount == 0 || amount > 100000u) return;
    std::string flow = flow_for_caller(lr);
    int64_t now = (int64_t)time(nullptr);
    auto unlocked =
        zcr_comp::on_toxin_spent(g_state, g_defs, (int64_t)amount, now, flow);
    g_dirty = true;
    logi("toxin spent: %u via %s (total %lld)", (unsigned)amount, flow.c_str(),
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

/* ---- recipe favorites (star persists via companion save) ---- */

/* register snapshot around the nested list-grow engine call */
struct Regs {
    uint32_t r[16] = {0};
    uint32_t cpsr = 0;
    void save(uc_engine *uc) {
        static const int kIds[16] = {
            UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3,
            UC_ARM_REG_R4,  UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,
            UC_ARM_REG_R8,  UC_ARM_REG_R9,  UC_ARM_REG_R10, UC_ARM_REG_R11,
            UC_ARM_REG_R12, UC_ARM_REG_SP,  UC_ARM_REG_LR,  UC_ARM_REG_PC};
        for (int i = 0; i < 16; i++) uc_reg_read(uc, kIds[i], &r[i]);
        uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    }
    void restore(uc_engine *uc) const {
        static const int kIds[16] = {
            UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3,
            UC_ARM_REG_R4,  UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,
            UC_ARM_REG_R8,  UC_ARM_REG_R9,  UC_ARM_REG_R10, UC_ARM_REG_R11,
            UC_ARM_REG_R12, UC_ARM_REG_SP,  UC_ARM_REG_LR,  UC_ARM_REG_PC};
        uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);
        for (int i = 0; i < 16; i++) {
            uint32_t v = r[i];
            if (i == 15) v |= 1u; /* Thumb */
            uc_reg_write(uc, kIds[i], &v);
        }
    }
};

/* RecipeBook::updateFavorite(bool) - fired by the cookbook star toggle.
 * The selected recipe row is at book+0x214 (verified: the tick's toggle
 * path reads it before calling). */
void on_update_fav(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    uint32_t book = 0, flag = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &book);
    uc_reg_read(uc, UC_ARM_REG_R1, &flag);
    uint8_t row = 0;
    if (!emu::read(uc, book + OFF_BOOK_SELROW, &row, 1) || row >= 250) return;
    std::string key = recipe_key_for(row);
    zcr_comp::set_favorite(g_state, key, row, flag != 0, (int64_t)time(nullptr));
    g_dirty = true;
    logi("favorite: row=%u key=%s -> %d", (unsigned)row, key.c_str(),
         flag != 0 ? 1 : 0);
    flush_locked(false);
}

/* RecipeBook::loadCookBook(bool) - runs when the cookbook builds. Reconcile
 * the persisted star flags with the recipe table before the UI reads them.
 * Observed engine behavior: the favorite flag (Recipe+0x1e) DOES round-trip
 * through the engine save, but with save-timing staleness (a toggle made
 * after the last engine save resurrects). Rules:
 *   - companion has records -> companion is the write-through truth
 *     (write both 1s and 0s for known rows);
 *   - companion is empty and the engine restored flags -> adopt them.
 * Also rebuild the engine's favorites slot list (14-cap, entries 1..count). */
void on_load_book(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    uint32_t book = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &book);

    uint32_t game = game_instance();
    uint32_t table = 0;
    if (game) emu::read(uc, game + OFF_GAME_RECIPES, &table, 4);
    if (!plausible_ptr(table)) return;

    /* only touch registry-known rows (bounded, safe reads/writes) */
    std::vector<int32_t> known;
    for (const auto &kv : g_row_ids) known.push_back(kv.first);
    std::sort(known.begin(), known.end());

    auto flag_at = [&](int32_t row) -> int {
        uint8_t b = 0;
        emu::read(uc, table + (uint32_t)row * RECIPE_STRIDE + OFF_RECIPE_FAV, &b,
                  1);
        return b ? 1 : 0;
    };

    /* engine-restored favorites */
    std::vector<int32_t> engine_favs;
    for (int32_t row : known)
        if (flag_at(row)) engine_favs.push_back(row);

    int adopted = 0, updated = 0, cleared = 0;
    if (g_state.favorites.empty() && !engine_favs.empty()) {
        int64_t now = (int64_t)time(nullptr);
        for (int32_t row : engine_favs) {
            std::string key = recipe_key_for(row);
            zcr_comp::set_favorite(g_state, key, row, true, now);
            adopted++;
        }
        g_dirty = true;
        logi("favorites adopted from engine save: %d", adopted);
    }

    std::vector<int32_t> rows;
    for (int32_t row : known) {
        std::string key = recipe_key_for(row);
        bool want = g_state.favorites.count(key) != 0;
        int have = flag_at(row);
        if (want && !have) {
            uint8_t one = 1;
            if (emu::write(uc, table + (uint32_t)row * RECIPE_STRIDE + OFF_RECIPE_FAV,
                           &one, 1))
                updated++;
            rows.push_back(row);
        } else if (want && have) {
            rows.push_back(row);
        } else if (!want && have) {
            uint8_t zero = 0;
            if (emu::write(uc, table + (uint32_t)row * RECIPE_STRIDE + OFF_RECIPE_FAV,
                           &zero, 1))
                cleared++;
        }
    }

    /* favorites slot list (mirror the toggle's add pattern) */
    uint32_t list = 0;
    if (emu::read(uc, book + OFF_BOOK_LIST, &list, 4) && plausible_ptr(list)) {
        int32_t n = (int32_t)rows.size();
        if (n > kMaxFavorites) n = kMaxFavorites;
        std::sort(rows.begin(), rows.end());
        uint32_t cap = 0;
        emu::read(uc, list + OFF_LIST_CAP, &cap, 4);
        uint32_t need = (uint32_t)n + 2;
        if (cap < need || cap > 4096) {
            uint32_t newcap = need > cap * 2 ? need : cap * 2;
            if (newcap < 4) newcap = 4;
            Regs snap;
            snap.save(uc);
            uint32_t words[1] = {newcap};
            jni_call_engine_r0_sp(uc, RVA_LIST_GROW | 1u, list + OFF_LIST_CAP,
                                  words, 1, (snap.r[13] - 256u) & ~7u);
            snap.restore(uc);
        }
        uint32_t array = 0;
        emu::read(uc, list + OFF_LIST_ARRAY, &array, 4);
        uint32_t cap2 = 0;
        emu::read(uc, list + OFF_LIST_CAP, &cap2, 4);
        if (plausible_ptr(array) && (uint32_t)n + 1 <= cap2) {
            int32_t count = n;
            emu::write(uc, list + OFF_LIST_COUNT, &count, 4);
            for (int32_t i = 0; i < n; i++)
                emu::write(uc, array + (uint32_t)(i + 1) * 4, &rows[i], 4);
        }
        logi("favorites reconcile: adopted=%d set=%d cleared=%d list=%d",
             adopted, updated, cleared, n);
    } else {
        logi("favorites reconcile: adopted=%d set=%d cleared=%d (no list)",
             adopted, updated, cleared);
    }
    if (adopted) flush_locked(false);
}

/* ---- zookdex purchase confirmation gate ----
 * Thumb cave at CAVE_PAGE (the dialog's yes-callback):
 *   push {lr}            ; B5 00
 *   ldr  r1, [pc, #8]    ; 49 02 -> saved purchase index at +0x0c
 *   ldr  r3, [pc, #8]    ; 4B 02 -> buyCharacter|1 at +0x10
 *   blx  r3              ; 47 98 -> LR = CAVE_PAGE+8|1 (re-entry marker)
 *   pop  {pc}            ; BD 00 -> return to ConfirmDialog::tick
 *   nop                  ; 46 C0
 *   .word index
 *   .word buyCharacter|1
 * The entry hook intercepts every buyCharacter call, queues the engine's own
 * ConfirmDialog, and redirects PC to LR (abort). On YES the cave re-enters
 * with the saved index; the hook sees LR == CAVE_PAGE+9 and lets it run. */
void write_cave(uc_engine *uc) {
    uint16_t code[6] = {0xB500, 0x4902, 0x4B02, 0x4798, 0xBD00, 0x46C0};
    emu::write(uc, CAVE_PAGE, code, sizeof(code));
    uint32_t zero = 0;
    emu::write(uc, CAVE_PAGE + CAVE_INDEX_OFF, &zero, 4);
    uint32_t fn = RVA_ZUKAN_BUY | 1u;
    emu::write(uc, CAVE_PAGE + 0x10, &fn, 4);
}

void on_zukan_buy(uc_engine *uc, uint64_t, int32_t, void *) {
    uint32_t lr = 0;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    if ((lr & ~1u) == CAVE_PAGE + CAVE_RET_OFF) return; /* confirmed re-entry */
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    uint32_t zukan = 0, index = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &zukan);
    uc_reg_read(uc, UC_ARM_REG_R1, &index);
    uint32_t dlg = 0;
    if (!plausible_ptr(zukan) ||
        !emu::read(uc, zukan + OFF_ZUKAN_DLG, &dlg, 4) || !plausible_ptr(dlg)) {
        logi("zukan buy gate: no dialog (zukan=0x%x dlg=0x%x); vanilla pass",
             zukan, dlg);
        return;
    }
    Regs snap;
    snap.save(uc);
    uint32_t sp = (snap.r[13] - 1024u) & ~7u;

    /* String scratch slots (4B each) live on the emu heap, reused per call */
    static uint32_t s_slots = 0;
    if (!s_slots) s_slots = emu::heap_alloc(uc, 16);
    if (!s_slots) {
        snap.restore(uc);
        return;
    }

    static const char *kMsg = "DO YOU WISH TO BUY THIS CHARACTER?";
    static const char *kYes = "BUY";
    static const char *kNo = "CANCEL";
    const char *texts[3] = {kMsg, kYes, kNo};
    for (int i = 0; i < 3; i++) {
        uint32_t t = emu::intern_string(uc, texts[i]);
        uint32_t args[1] = {t};
        jni_call_engine_r0_sp(uc, RVA_STR_CTOR | 1u, s_slots + (uint32_t)i * 4,
                              args, 1, sp);
    }

    emu::write(uc, CAVE_PAGE + CAVE_INDEX_OFF, &index, 4);
    /* String is non-trivial: by-value args are addresses of temporaries */
    uint32_t words[5] = {s_slots, s_slots + 4, s_slots + 8,
                         CAVE_PAGE | 1u, zukan};
    jni_call_engine_r0_sp(uc, RVA_CONFIRM_SHOW | 1u, dlg, words, 5, sp);

    for (int i = 0; i < 3; i++)
        jni_call_engine_r0_sp(uc, RVA_STR_DTOR | 1u, s_slots + (uint32_t)i * 4,
                              nullptr, 0, sp);

    snap.restore(uc);
    uint32_t pc = lr | 1u;
    uc_reg_write(uc, UC_ARM_REG_PC, &pc); /* abort the unconfirmed call */
    logi("zukan buy: confirm queued (index=%u zukan=0x%x)", index, zukan);
}

/* ---- QoL hooks ---- */

/* ServeTask::tick placement block (RVA_SERVE_PLACE). The engine places the
 * carried dish without checking the target's in-use flag (CafeObject+0x2e),
 * so a second dish lands on a spot that already has one and the new dish
 * vanishes (the overwrite bug). When the spot is busy, divert to the
 * engine's own return-dish-to-counter path (RVA_SERVE_ABORT). Registers are
 * untouched: the abort path sees the same frame state the engine would. */
void on_serve_place(uc_engine *uc, uint64_t, int32_t, void *) {
    uint32_t task = 0;
    uc_reg_read(uc, UC_ARM_REG_R4, &task);
    uint32_t target = 0;
    if (!plausible_ptr(task) ||
        !emu::read(uc, task + OFF_TASK_TARGET, &target, 4) ||
        !plausible_ptr(target))
        return;
    uint8_t busy = 0;
    emu::read(uc, target + OFF_OBJ_BUSY, &busy, 1);
    if (busy == 0) return; /* empty spot: place normally */
    uint32_t pc = RVA_SERVE_ABORT | 1u;
    uc_reg_write(uc, UC_ARM_REG_PC, &pc);
    logi("serve guard: spot busy - dish returned to counter (task=0x%x)", task);
}

/* GameStateCafe::characterCanFollow - the staff/serve eligibility gate in
 * tickStaff (probabilistic in the original). Auto-serve: always eligible. */
void on_can_follow(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    static int64_t s_last = 0, s_count = 0;
    s_count++;
    int64_t now = (int64_t)time(nullptr);
    if (now - s_last >= 60) {
        s_last = now;
        logi("auto-serve gate: forced %lld eligible checks", (long long)s_count);
    }
    uint32_t one = 1, lr = 0;
    uc_reg_write(uc, UC_ARM_REG_R0, &one);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uint32_t pc = lr | 1u;
    uc_reg_write(uc, UC_ARM_REG_PC, &pc);
}

/* GameStateCafe::cook entry - record the add-on (variant) the player cooks
 * each recipe with, so auto-cook replays the same choice. try_lock: the
 * auto-cook driver itself nested-calls cook while holding g_mu - those
 * calls skip recording (their variant is already the stored one). */
void on_cook_start(uc_engine *uc, uint64_t, int32_t, void *) {
    std::unique_lock<std::mutex> lk(g_mu, std::try_to_lock);
    if (!lk.owns_lock() || !g_ready) return;
    uint32_t row = 0, variant = 0;
    uc_reg_read(uc, UC_ARM_REG_R2, &row);
    uc_reg_read(uc, UC_ARM_REG_R3, &variant);
    if (row >= 250) return;
    int32_t v = (int32_t)(variant & 0xff);
    auto it = g_state.cook_variant.find((int32_t)row);
    if (it == g_state.cook_variant.end() || it->second != v) {
        g_state.cook_variant[(int32_t)row] = v;
        g_dirty = true;
        logi("cook add-on: row=%u variant=%d", row, v);
    }
}

/* Preferred chef: the character the player selects ("YOU" panel) - pinned
 * so the cook/serve tasks always go to the chef even after deselect. */
uint32_t g_pref_chef = 0;

void on_select_char(uc_engine *uc, uint64_t, int32_t, void *) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready) return;
    uint32_t chr = 0;
    uc_reg_read(uc, UC_ARM_REG_R1, &chr);
    if (!plausible_ptr(chr) || chr == g_pref_chef) return;
    g_pref_chef = chr;
    /* locate the avatar in the character array for zero-tap persistence */
    uint32_t game = game_instance(), state = 0, arr = 0, count = 0;
    int idx = -1;
    if (plausible_ptr(game) &&
        emu::read(uc, game + OFF_GAME_STATE, &state, 4) && plausible_ptr(state)) {
        emu::read(uc, state + OFF_STATE_CHARARR, &arr, 4);
        emu::read(uc, state + OFF_STATE_CHARS, &count, 4);
        if (plausible_ptr(arr) && count < 4096) {
            for (uint32_t i = 0; i < count; i++) {
                uint32_t e = 0;
                emu::read(uc, arr + i * 4, &e, 4);
                if (e == chr) { idx = (int)i; break; }
            }
        }
    }
    logi("chef pinned: char=0x%x idx=%d/%u", chr, idx, count);
}

/* Auto-cook driver (from the render tick). Idle stove -> cook the most
 * recently cooked recipe; when coins are short, fire a "needs you" Android
 * notification (throttled). The engine cook call performs the real cost
 * check + deduction; this pre-check only decides between cook/notify. */
void qol_tick_impl() {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_ready || !g_uc) return;
    static int64_t s_last_cook = 0, s_last_notif = 0;
    int64_t now = (int64_t)time(nullptr);
    if (now - s_last_cook < kAutoCookGapSec) return;
    uc_engine *uc = g_uc;

    uint32_t game = game_instance();
    uint32_t state = 0, cafe = 0, zgame = 0, head = 0;
    bool state_ok =
        plausible_ptr(game) &&
        emu::read(uc, game + OFF_GAME_STATE, &state, 4) && plausible_ptr(state) &&
        emu::read(uc, state + OFF_STATE_GAME, &zgame, 4) && plausible_ptr(zgame) &&
        emu::read(uc, state + OFF_STATE_CAFE, &cafe, 4) && plausible_ptr(cafe) &&
        emu::read(uc, cafe + 0x38, &head, 4) &&
        (head == 0 || plausible_ptr(head)); /* furniture list ready */
    if (!state_ok) {
        static int64_t s_dbg = 0;
        if (now - s_dbg >= 30) {
            s_dbg = now;
            logi("auto-cook wait: game=0x%x state=0x%x zgame=0x%x cafe=0x%x",
                 game, state, zgame, cafe);
        }
        return;
    }

    int32_t row = g_state.last_cooked_row;
    if (row < 0 || row >= 250) {
        /* migrated save without the field: repeat the most recently
         * cooked recipe from the per-recipe stats */
        int64_t best_ts = 0;
        for (const auto &kv : g_state.recipes) {
            if (kv.second.row >= 0 && kv.second.row < 250 &&
                kv.second.last_ts >= best_ts) {
                best_ts = kv.second.last_ts;
                row = kv.second.row;
            }
        }
    }
    if (row < 0 || row >= 250) return;

    static int64_t s_last_status = 0;
    bool do_status = (now - s_last_status >= 60);
    if (do_status) s_last_status = now;

    Regs snap;
    snap.save(uc);
    uint32_t sp = (snap.r[13] - 512u) & ~7u;

    /* repeat recipe with the player's chosen add-on (fallback: BULK) */
    int32_t variant = kBulkVariant;
    auto vit = g_state.cook_variant.find(row);
    if (vit != g_state.cook_variant.end()) variant = vit->second;

    /* engine-accurate stove pick (exitRecipeBook's own auto-pick):
     * createDistanceSortedStoveList(cafe, recipe.cookType, 0) and the
     * first appliance with the +0x2e busy flag clear. The generic
     * furniture list previously matched the wrong appliance (fridge). */
    uint32_t table = 0, recipe = 0;
    emu::read(uc, game + OFF_GAME_RECIPES, &table, 4);
    if (plausible_ptr(table))
        recipe = table + (uint32_t)row * RECIPE_STRIDE;
    uint8_t cook_type = 0;
    if (recipe) emu::read(uc, recipe + OFF_RECIPE_COOKTYPE, &cook_type, 1);
    uint32_t list_words[2] = {cook_type, 0u};
    uint32_t list = jni_call_engine_r0_sp(uc, RVA_STOVE_LIST | 1u, cafe,
                                          list_words, 2, sp);
    uint32_t stove = 0, busy = 0;
    if (plausible_ptr(list)) {
        uint32_t node = 0, guard = 0;
        emu::read(uc, list + 8, &node, 4);
        while (plausible_ptr(node) && guard++ < 256) {
            uint32_t item = 0, next = 0;
            emu::read(uc, node, &item, 4);
            emu::read(uc, node + 8, &next, 4);
            if (plausible_ptr(item)) {
                uint8_t flag = 1;
                emu::read(uc, item + OFF_OBJ_BUSY, &flag, 1);
                if (flag == 0) {
                    if (!stove) stove = item;
                } else if (!busy) {
                    busy = item;
                }
            }
            jni_call_engine_r0_sp(uc, RVA_FREE | 1u, node, nullptr, 0, sp);
            node = next;
        }
        jni_call_engine_r0_sp(uc, RVA_FREE | 1u, list, nullptr, 0, sp);
    }
    /* Chef resolution: the selected character ("YOU" panel) is the chef.
     * Zero-tap fallback: the avatar is character-array[0] (created first in
     * GameStateChooseChef), so fresh sessions pin him automatically. */
    uint32_t chef = 0;
    emu::read(uc, state + OFF_STATE_SELCHAR, &chef, 4);
    if (!plausible_ptr(chef) && plausible_ptr(g_pref_chef))
        chef = g_pref_chef;
    if (!plausible_ptr(chef)) {
        uint32_t arr = 0;
        if (emu::read(uc, state + OFF_STATE_CHARARR, &arr, 4) &&
            plausible_ptr(arr))
            emu::read(uc, arr, &chef, 4);
        if (plausible_ptr(chef))
            logi("chef auto-pinned: char=0x%x (array[0])", chef);
    }
    if (plausible_ptr(chef)) {
        g_pref_chef = chef;
        uint32_t sel = 0;
        emu::read(uc, state + OFF_STATE_SELCHAR, &sel, 4);
        if (sel != chef) emu::write(uc, state + OFF_STATE_SELCHAR, &chef, 4);
    }

    /* keep the engine's own chef-loop pointing at the same stove */
    uint32_t target = stove ? stove : busy;
    if (target) {
        uint32_t loop = 0;
        emu::read(uc, state + OFF_STATE_LOOPSTOVE, &loop, 4);
        if (loop != target) {
            emu::write(uc, state + OFF_STATE_SELSTOVE, &target, 4);
            emu::write(uc, state + OFF_STATE_LOOPSTOVE, &target, 4);
        }
    }

    /* Direct task assignment to the chef. The engine's per-frame chef state
     * machine can leave him idle (health/tutorial gating); assign the
     * cook/carry/clean task straight to him whenever he is idle. */
    auto chef_idle = [&]() -> bool {
        uint32_t data = 0;
        int32_t v = 0;
        if (emu::read(uc, chef + OFF_CHAR_DATA, &data, 4) && plausible_ptr(data))
            emu::read(uc, data + OFF_CHARDATA_IDLE, &v, 4);
        return v <= 0;
    };
    auto assign_task = [&](uint32_t kind, uint32_t tgt) {
        static uint32_t s_kind = 9, s_tgt = 0;
        static int64_t s_ts = 0;
        if (kind == s_kind && tgt == s_tgt && now - s_ts < 30) return;
        uint32_t alloc = kind == 0 ? 0x30u : (kind == 1 ? 0x44u : 0x40u);
        uint32_t task =
            jni_call_engine_r0_sp(uc, RVA_NEW | 1u, alloc, nullptr, 0, sp);
        if (!plausible_ptr(task)) return;
        uint32_t a_csc[3] = {chef, tgt, cafe};
        uint32_t a_ccs[3] = {chef, cafe, tgt};
        uint32_t ctor = kind == 0 ? RVA_COOK_TASK
                       : (kind == 1 ? RVA_DISH_TASK : RVA_CLEAN_TASK);
        jni_call_engine_r0_sp(uc, ctor | 1u, task, kind == 0 ? a_csc : a_ccs,
                              3, sp);
        uint32_t aargs[1] = {task};
        jni_call_engine_r0_sp(uc, RVA_ASSIGN_TASK | 1u, chef, aargs, 1, sp);
        s_kind = kind;
        s_tgt = tgt;
        s_ts = now;
        logi("chef task: %s stove=0x%x chef=0x%x",
             kind == 0 ? "cook" : (kind == 1 ? "carry-to-counter" : "clean"),
             tgt, chef);
    };

    if (!stove) {
        /* busy stove: carry the done dish (auto-serve), clean burns, tend */
        if (busy && chef && plausible_ptr(chef) && chef_idle()) {
            uint32_t burnt =
                jni_call_engine_r0_sp(uc, RVA_IS_BURNED | 1u, busy, nullptr, 0, sp);
            uint32_t done =
                jni_call_engine_r0_sp(uc, RVA_IS_DONE | 1u, busy, nullptr, 0, sp);
            assign_task(burnt ? 2u : (done ? 1u : 0u), busy);
        }
        if (do_status) {
            uint32_t f = 0, el = 0, fl = 0;
            if (busy) {
                emu::read(uc, busy + 0x60, &f, 4);
                emu::read(uc, busy + 0x68, &el, 4);
                uint8_t b = 0;
                emu::read(uc, busy + 0x2e, &b, 1);
                fl = b;
            }
            logi("auto-cook status: row=%d no idle stove (busy=0x%x food=0x%x "
                 "elapsed=%u flag=%u)", row, busy, f, el, fl);
        }
        snap.restore(uc);
        return;
    }
    if (do_status) logi("auto-cook status: row=%d stove=0x%x", row, stove);

    /* cost of the repeat recipe vs coins */
    uint32_t cost = 0;
    if (recipe) {
        uint32_t cost_args[1] = {(uint32_t)(variant & 0xff)};
        cost = jni_call_engine_r0_sp(uc, RVA_RECIPE_COST | 1u, recipe,
                                     cost_args, 1, sp);
    }
    uint32_t money = 0;
    emu::read(uc, zgame + OFF_GAME_MONEY, &money, 4);

    if (cost > money) {
        if (now - s_last_notif >= kNotifGapSec) {
            s_last_notif = now;
            uint32_t msg = emu::intern_string(
                uc, "Your cafe needs you! Not enough coins to keep cooking.");
            uint32_t n_args[3] = {msg, 0u, 0u}; /* extra=0, delay=0s */
            jni_call_engine_r0_sp(uc, RVA_SEND_NOTIF | 1u, 0u, n_args, 3, sp);
            logi("auto-cook: unaffordable (row=%u cost=%u money=%u) - "
                 "notified user", row, cost, money);
        }
        snap.restore(uc);
        return;
    }

    /* CapcomDate now + two empty Strings, then cook. String is non-trivial:
     * by-value args are addresses of caller-built temporaries. */
    uint32_t scratch = emu::heap_alloc(uc, 32);
    if (!scratch) {
        snap.restore(uc);
        return;
    }
    jni_call_engine_r0_sp(uc, RVA_DATE_NOW | 1u, scratch, nullptr, 0, sp);
    uint32_t zero = 0;
    jni_call_engine_r0_sp(uc, RVA_STR_FROM_INT | 1u, scratch + 8, &zero, 1, sp);
    jni_call_engine_r0_sp(uc, RVA_STR_FROM_INT | 1u, scratch + 12, &zero, 1, sp);
    uint32_t date0 = 0, date1 = 0;
    emu::read(uc, scratch, &date0, 4);
    emu::read(uc, scratch + 4, &date1, 4);
    uint32_t cook_words[7] = {stove, (uint32_t)row, (uint32_t)(variant & 0xff),
                              date0, date1, scratch + 8, scratch + 12};
    jni_call_engine_r0_sp(uc, RVA_COOK | 1u, state, cook_words, 7, sp);
    emu::heap_free(uc, scratch);
    s_last_cook = now;
    uint32_t money_after = 0, food_after = 0;
    emu::read(uc, zgame + OFF_GAME_MONEY, &money_after, 4);
    emu::read(uc, stove + OFF_STOVE_FOOD, &food_after, 4);
    snap.restore(uc);
    logi("auto-cook: stove=0x%x row=%u variant=%d cost=%u money=%u->%u "
         "foodstack=0x%x", stove, row, (int)variant, cost, money, money_after,
         food_after);
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

    uc_hook h1 = 0, h2 = 0, h3 = 0, h4 = 0, h5 = 0, h6 = 0;
    uc_err e1 = uc_hook_add(uc, &h1, UC_HOOK_CODE, (void *)on_cooked, nullptr,
                            ENG_ADDR(RVA_COOKED), ENG_ADDR(RVA_COOKED));
    uc_err e2 = uc_hook_add(uc, &h2, UC_HOOK_CODE, (void *)on_spend, nullptr,
                            ENG_ADDR(RVA_SPEND), ENG_ADDR(RVA_SPEND));
    uc_err e3 = uc_hook_add(uc, &h3, UC_HOOK_CODE, (void *)on_add, nullptr,
                            ENG_ADDR(RVA_ADD), ENG_ADDR(RVA_ADD));
    uc_err e4 = uc_hook_add(uc, &h4, UC_HOOK_CODE, (void *)on_update_fav,
                            nullptr, ENG_ADDR(RVA_UPDATE_FAV),
                            ENG_ADDR(RVA_UPDATE_FAV));
    uc_err e5 = uc_hook_add(uc, &h5, UC_HOOK_CODE, (void *)on_load_book,
                            nullptr, ENG_ADDR(RVA_LOAD_BOOK),
                            ENG_ADDR(RVA_LOAD_BOOK));
    if (e1 != UC_ERR_OK || e2 != UC_ERR_OK || e3 != UC_ERR_OK ||
        e4 != UC_ERR_OK || e5 != UC_ERR_OK) {
        logi("hook add failed: %s / %s / %s / %s / %s", uc_strerror(e1),
             uc_strerror(e2), uc_strerror(e3), uc_strerror(e4),
             uc_strerror(e5));
        return false;
    }
    logi("hooks installed (cooked@0x%x spend@0x%x add@0x%x fav@0x%x book@0x%x)",
         RVA_COOKED, RVA_SPEND, RVA_ADD, RVA_UPDATE_FAV, RVA_LOAD_BOOK);

    /* zookdex purchase gate: cave first, hook only when the cave is live
     * (without it the gate could abort purchases without re-entry) */
    uc_err em = uc_mem_map(uc, CAVE_PAGE, 0x1000, UC_PROT_ALL);
    if (em != UC_ERR_OK) {
        logi("zukan gate disabled: cave map failed: %s", uc_strerror(em));
    } else {
        write_cave(uc);
        uc_err e6 = uc_hook_add(uc, &h6, UC_HOOK_CODE, (void *)on_zukan_buy,
                                nullptr, ENG_ADDR(RVA_ZUKAN_BUY),
                                ENG_ADDR(RVA_ZUKAN_BUY));
        if (e6 != UC_ERR_OK) {
            logi("zukan gate disabled: hook add failed: %s", uc_strerror(e6));
        } else {
            logi("zukan buy gate installed (buy@0x%x cave@0x%x)",
                 RVA_ZUKAN_BUY, CAVE_PAGE);
        }
    }

    /* QoL hooks: serve guard (overwrite fix) + auto-serve gate + cook
     * add-on recording + chef pin */
    uc_hook h7 = 0, h8 = 0, h9 = 0, h10 = 0;
    uc_err e7 = uc_hook_add(uc, &h7, UC_HOOK_CODE, (void *)on_serve_place,
                            nullptr, ENG_ADDR(RVA_SERVE_PLACE),
                            ENG_ADDR(RVA_SERVE_PLACE));
    uc_err e8 = uc_hook_add(uc, &h8, UC_HOOK_CODE, (void *)on_can_follow,
                            nullptr, ENG_ADDR(RVA_CAN_FOLLOW),
                            ENG_ADDR(RVA_CAN_FOLLOW));
    uc_err e9 = uc_hook_add(uc, &h9, UC_HOOK_CODE, (void *)on_cook_start,
                            nullptr, ENG_ADDR(RVA_COOK),
                            ENG_ADDR(RVA_COOK));
    uc_err e10 = uc_hook_add(uc, &h10, UC_HOOK_CODE, (void *)on_select_char,
                             nullptr, ENG_ADDR(RVA_SELECT_CHAR),
                             ENG_ADDR(RVA_SELECT_CHAR));
    if (e7 != UC_ERR_OK || e8 != UC_ERR_OK || e9 != UC_ERR_OK ||
        e10 != UC_ERR_OK) {
        logi("qol hooks failed: %s / %s / %s / %s", uc_strerror(e7),
             uc_strerror(e8), uc_strerror(e9), uc_strerror(e10));
    } else {
        logi("qol hooks installed (serve guard@0x%x auto-serve@0x%x "
             "cook-rec@0x%x chef-pin@0x%x)", RVA_SERVE_PLACE, RVA_CAN_FOLLOW,
             RVA_COOK, RVA_SELECT_CHAR);
    }
    flush_locked(false);
    return true;
}

void flush(bool force) {
    std::lock_guard<std::mutex> lk(g_mu);
    flush_locked(force);
}

void qol_tick() { qol_tick_impl(); }

}  // namespace zcr_companion
