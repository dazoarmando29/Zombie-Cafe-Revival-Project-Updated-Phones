/* Zombie Café Revival companion save: recipe mastery + achievements state.
 *
 * Pure logic + JSON (de)serialization, no engine/emulator dependencies so
 * the same code is unit-testable off-device. The host-side manager
 * (zcr_companion.cpp) feeds it events from engine hooks and persists the
 * state to a versioned local file that never touches the engine save
 * format (append-only, monotonic counters; reward claims are latched).
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace zcr_comp {

/* ---- achievement definitions (loaded from zcr_achievements.json) ---- */

enum class AchType {
    CookTotal,       /* total dishes cooked                >= threshold   */
    CookRecipe,      /* param: recipe zcrId (or row:N)     >= threshold   */
    DistinctRecipes, /* distinct recipes ever cooked       >= threshold   */
    MasteryRecipe,   /* param: recipe key, param2: level   >= param2      */
    MasteryCount,    /* recipes with mastery >= param2     >= threshold   */
    ToxinSpent,      /* cumulative toxin spent             >= threshold   */
    ToxinEarned,     /* cumulative toxin earned            >= threshold   */
};

struct AchDef {
    std::string id;     /* "ach.cook_10" - stable, append-only */
    std::string title;
    std::string desc;
    std::string param;  /* recipe key for CookRecipe/MasteryRecipe */
    AchType type = AchType::CookTotal;
    int32_t param2 = 0; /* mastery level for Mastery* */
    int64_t threshold = 0;
    int32_t reward_toxin = 0;
};

struct Defs {
    std::vector<AchDef> achievements;
    std::vector<int64_t> mastery_levels; /* e.g. {10, 50, 200} */
};

/* ---- persisted state ---- */

struct RecipeStat {
    std::string zcr_id; /* "recipe.brain_burger" or "" when unknown */
    int32_t row = -1;   /* foodData row, -1 unknown */
    int64_t cooked = 0;
    int64_t first_ts = 0;
    int64_t last_ts = 0;
};

struct AchState {
    int64_t progress = 0;
    bool unlocked = false;
    int64_t unlocked_ts = 0;
    bool claimed = false;
    int64_t claimed_ts = 0;
};

struct State {
    int32_t version = 1;
    int64_t created_ts = 0;
    int64_t updated_ts = 0;
    int64_t total_cooked = 0;
    int64_t toxin_spent = 0;
    int64_t toxin_earned = 0;
    /* key = zcrId when known else "row:N" */
    std::map<std::string, RecipeStat> recipes;
    /* key = achievement id */
    std::map<std::string, AchState> achievements;
};

/* Mastery level for a cook count: 0 = none, 1..N = tier (thresholds in
 * Defs::mastery_levels, ascending). */
int mastery_level(const Defs &d, int64_t cooked);

/* Recipe key helper: prefer the stable zcr id, fall back to "row:N". */
std::string recipe_key(const std::string &zcr_id, int32_t row);

/* Event handlers: mutate state, recompute achievement progress and return
 * the ids of achievements newly unlocked by this event (each id is returned
 * exactly once across the lifetime of the state). */
std::vector<std::string> on_dish_cooked(State &s, const Defs &d,
                                        const std::string &key, int32_t row,
                                        int64_t now);
std::vector<std::string> on_toxin_spent(State &s, const Defs &d, int64_t amount,
                                        int64_t now);
std::vector<std::string> on_toxin_earned(State &s, const Defs &d,
                                         int64_t amount, int64_t now);

/* Recompute achievement progress from the aggregate state (idempotent).
 * Unlocked flags latch: an unlocked achievement never re-triggers. */
std::vector<std::string> evaluate(State &s, const Defs &d, int64_t now);

/* Reward claiming: persist-then-grant order (the caller must flush the
 * state with claimed=true BEFORE granting the toxin in-engine). */
void mark_claimed(State &s, const std::string &id, int64_t now);
int32_t achievement_reward(const Defs &d, const std::string &id);

/* ---- JSON ---- */

std::string serialize(const State &s);
bool parse_state(const std::string &json, State *out, std::string *err);
bool parse_defs(const std::string &json, Defs *out, std::string *err);

/* Parse zcr_registry.lock.json: row -> zcrId for one tracked file
 * (e.g. "foodData.bin.mid.json"). Used to key recipe stats by stable id. */
bool parse_registry_rows(const std::string &json, const std::string &file,
                         std::map<int32_t, std::string> *row_to_id,
                         std::string *err);

}  // namespace zcr_comp
