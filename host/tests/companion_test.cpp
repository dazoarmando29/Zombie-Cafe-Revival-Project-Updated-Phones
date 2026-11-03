/* Unit tests for the companion save (recipe mastery + achievements).
 *
 * Build (NDK clang, self-contained binary) and run on an Android device:
 *   see host/tests/run_companion_tests.ps1
 *
 * Covers: JSON round-trips + malformed input, mastery thresholds, cook
 * counting (ZcrId + row fallback keys), achievement evaluation (idempotent
 * unlocks, latch semantics), reward claim persist-then-grant ordering,
 * registry-lock parsing, and anti-exploit monotonicity guards. */
#include "../companion_state.h"
#include "../event_state.h"
#include "../boss_state.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace zcr_comp;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        g_checks++;                                                        \
        long long va = (long long)(a);                                     \
        long long vb = (long long)(b);                                     \
        if (va != vb) {                                                    \
            g_failures++;                                                  \
            printf("[FAIL] %s:%d: %s == %s (%lld vs %lld)\n", __FILE__,    \
                   __LINE__, #a, #b, va, vb);                              \
        }                                                                  \
    } while (0)

static Defs make_defs() {
    const char *json =
        "{\"version\":1,"
        "\"mastery\":{\"levels\":[10,50,200]},"
        "\"achievements\":["
        "{\"id\":\"ach.cook_first\",\"title\":\"First\",\"type\":\"cook_total\","
        "\"threshold\":1,\"reward_toxin\":1},"
        "{\"id\":\"ach.cook_5\",\"title\":\"Five\",\"type\":\"cook_total\","
        "\"threshold\":5,\"reward_toxin\":2},"
        "{\"id\":\"ach.distinct_2\",\"title\":\"Two\",\"type\":\"distinct_recipes\","
        "\"threshold\":2,\"reward_toxin\":1},"
        "{\"id\":\"ach.r_burger\",\"title\":\"Burger\",\"type\":\"cook_recipe\","
        "\"param\":\"recipe.brain_burger\",\"threshold\":1,\"reward_toxin\":1},"
        "{\"id\":\"ach.mastery_burger\",\"title\":\"Master\",\"type\":\"mastery_recipe\","
        "\"param\":\"recipe.brain_burger\",\"param2\":1,\"threshold\":1,\"reward_toxin\":2},"
        "{\"id\":\"ach.mastery_any2\",\"title\":\"TwoMastery\",\"type\":\"mastery_count\","
        "\"param2\":1,\"threshold\":2,\"reward_toxin\":3},"
        "{\"id\":\"ach.spend_10\",\"title\":\"Spend\",\"type\":\"toxin_spent\","
        "\"threshold\":10,\"reward_toxin\":1},"
        "{\"id\":\"ach.earn_3\",\"title\":\"Earn\",\"type\":\"toxin_earned\","
        "\"threshold\":3,\"reward_toxin\":1}"
        "]}";
    Defs d;
    std::string err;
    bool ok = parse_defs(json, &d, &err);
    CHECK(ok);
    CHECK_EQ(d.achievements.size(), 8);
    CHECK_EQ(d.mastery_levels.size(), 3);
    return d;
}

static void test_mastery() {
    Defs d;
    d.mastery_levels = {10, 50, 200};
    CHECK_EQ(mastery_level(d, 0), 0);
    CHECK_EQ(mastery_level(d, 9), 0);
    CHECK_EQ(mastery_level(d, 10), 1);
    CHECK_EQ(mastery_level(d, 49), 1);
    CHECK_EQ(mastery_level(d, 50), 2);
    CHECK_EQ(mastery_level(d, 199), 2);
    CHECK_EQ(mastery_level(d, 200), 3);
    CHECK_EQ(mastery_level(d, 100000), 3);
    /* empty thresholds: everything level 0 */
    Defs e;
    CHECK_EQ(mastery_level(e, 1000), 0);
}

static void test_recipe_key() {
    CHECK(recipe_key("recipe.brain_burger", 217) == "recipe.brain_burger");
    CHECK(recipe_key("", 217) == "row:217");
    CHECK(recipe_key("", 0) == "row:0");
}

static void test_cook_counting() {
    Defs d = make_defs();
    State s;
    int64_t now = 1000;

    auto u1 = on_dish_cooked(s, d, "recipe.brain_burger", 217, now);
    CHECK_EQ(s.total_cooked, 1);
    CHECK_EQ(s.recipes["recipe.brain_burger"].cooked, 1);
    CHECK_EQ(s.recipes["recipe.brain_burger"].row, 217);
    CHECK(s.recipes["recipe.brain_burger"].first_ts == now);
    /* cook_first unlocks on the first dish */
    bool got_first = false;
    for (const auto &id : u1)
        if (id == "ach.cook_first") got_first = true;
    CHECK(got_first);

    on_dish_cooked(s, d, "recipe.brain_burger", 217, now + 1);
    on_dish_cooked(s, d, "recipe.brain_burger", 217, now + 2);
    CHECK_EQ(s.total_cooked, 3);
    CHECK_EQ(s.recipes["recipe.brain_burger"].cooked, 3);
    CHECK(s.recipes["recipe.brain_burger"].last_ts == now + 2);

    /* unknown-recipe fallback key */
    on_dish_cooked(s, d, "row:7", 7, now + 3);
    CHECK_EQ(s.recipes["row:7"].cooked, 1);
    CHECK_EQ(s.total_cooked, 4);

    /* cook_first must NOT re-trigger on later cooks / evaluations */
    auto u2 = on_dish_cooked(s, d, "row:8", 8, now + 4);
    for (const auto &id : u2) CHECK(id != "ach.cook_first");
    u2 = evaluate(s, d, now + 5);
    for (const auto &id : u2) CHECK(id != "ach.cook_first");
    CHECK(s.achievements["ach.cook_first"].unlocked);
}

static void test_distinct_and_mastery_achievements() {
    Defs d = make_defs();
    State s;
    int64_t now = 1;

    on_dish_cooked(s, d, "recipe.a", 1, now);
    auto u = on_dish_cooked(s, d, "recipe.b", 2, now + 1);
    bool got_distinct = false;
    for (const auto &id : u)
        if (id == "ach.distinct_2") got_distinct = true;
    CHECK(got_distinct);

    /* mastery_recipe: 10 cooks of brain burger -> level 1 */
    bool got_mastery = false, got_mastery_any2 = false;
    for (int i = 0; i < 10; i++) {
        auto un = on_dish_cooked(s, d, "recipe.brain_burger", 217, now + 2 + i);
        for (const auto &id : un) {
            if (id == "ach.mastery_burger") got_mastery = true;
            if (id == "ach.mastery_any2") got_mastery_any2 = true;
        }
    }
    CHECK(got_mastery);
    CHECK_EQ(mastery_level(d, s.recipes["recipe.brain_burger"].cooked), 1);
    /* mastery_count(>=1, threshold 2): brain burger + recipe.a/recipe.b at
     * level 0 -> only 1 recipe at level >= 1 -> NOT unlocked yet */
    CHECK(!got_mastery_any2);
    /* level up recipe.a to 10 cooks -> now 2 recipes at mastery >= 1;
     * the unlock fires during the cook that crosses the threshold */
    for (int i = 0; i < 9; i++) {
        auto un = on_dish_cooked(s, d, "recipe.a", 1, now + 20 + i);
        for (const auto &id : un)
            if (id == "ach.mastery_any2") got_mastery_any2 = true;
    }
    CHECK(got_mastery_any2);
    auto u3 = evaluate(s, d, now + 40);
    for (const auto &id : u3) CHECK(id != "ach.mastery_any2");
}

static void test_toxin_events() {
    Defs d = make_defs();
    State s;
    int64_t now = 5;

    auto u = on_toxin_spent(s, d, 4, now);
    CHECK_EQ(s.toxin_spent, 4);
    CHECK_EQ(u.size(), 0);
    u = on_toxin_spent(s, d, 6, now + 1);
    CHECK_EQ(s.toxin_spent, 10);
    bool got = false;
    for (const auto &id : u)
        if (id == "ach.spend_10") got = true;
    CHECK(got);

    /* zero / negative amounts are ignored */
    on_toxin_spent(s, d, 0, now + 2);
    on_toxin_spent(s, d, -5, now + 2);
    CHECK_EQ(s.toxin_spent, 10);

    auto u_earn = on_toxin_earned(s, d, 3, now + 3);
    CHECK_EQ(s.toxin_earned, 3);
    bool got_earn = false;
    for (const auto &id : u_earn)
        if (id == "ach.earn_3") got_earn = true;
    CHECK(got_earn);
    auto u2 = on_toxin_earned(s, d, 0, now + 4);
    CHECK_EQ(u2.size(), 0);
    u2 = evaluate(s, d, now + 5);
    for (const auto &id : u2) CHECK(id != "ach.earn_3");
}

static void test_claim_latch() {
    Defs d = make_defs();
    State s;
    on_dish_cooked(s, d, "recipe.a", 1, 100);
    CHECK(s.achievements["ach.cook_first"].unlocked);
    CHECK(!s.achievements["ach.cook_first"].claimed);
    mark_claimed(s, "ach.cook_first", 101);
    CHECK(s.achievements["ach.cook_first"].claimed);
    CHECK_EQ(s.achievements["ach.cook_first"].claimed_ts, 101);
    mark_claimed(s, "ach.cook_first", 200);
    CHECK_EQ(s.achievements["ach.cook_first"].claimed_ts, 200);
    /* unlock latch: more cooks never re-unlock */
    for (int i = 0; i < 100; i++) on_dish_cooked(s, d, "recipe.a", 1, 300 + i);
    auto u = evaluate(s, d, 400);
    for (const auto &id : u) CHECK(id != "ach.cook_first");
    CHECK_EQ(achievement_reward(d, "ach.cook_first"), 1);
    CHECK_EQ(achievement_reward(d, "ach.mastery_burger"), 2);
    CHECK_EQ(achievement_reward(d, "ach.unknown_id"), 0);
}

static void test_state_roundtrip() {
    Defs d = make_defs();
    State s;
    s.created_ts = 1000;
    on_dish_cooked(s, d, "recipe.brain_burger", 217, 1001);
    on_dish_cooked(s, d, "recipe.brain_burger", 217, 1002);
    on_dish_cooked(s, d, "row:7", 7, 1003);
    CHECK_EQ(s.last_cooked_row, 7);
    s.cook_variant[0] = 5;
    s.cook_variant[7] = 3;
    on_toxin_spent(s, d, 4, 1004);
    on_toxin_earned(s, d, 3, 1005);
    mark_claimed(s, "ach.cook_first", 1006);

    std::string json = serialize(s);
    State back;
    std::string err;
    CHECK(parse_state(json, &back, &err));
    CHECK_EQ(back.version, 1);
    CHECK_EQ(back.created_ts, 1000);
    CHECK_EQ(back.total_cooked, 3);
    CHECK_EQ(back.toxin_spent, 4);
    CHECK_EQ(back.toxin_earned, 3);
    CHECK_EQ(back.last_cooked_row, 7);
    CHECK_EQ((int)back.cook_variant.size(), 2);
    CHECK_EQ(back.cook_variant[0], 5);
    CHECK_EQ(back.cook_variant[7], 3);
    CHECK_EQ(back.recipes.size(), 2);
    CHECK_EQ(back.recipes["recipe.brain_burger"].cooked, 2);
    CHECK(back.recipes["recipe.brain_burger"].zcr_id == "recipe.brain_burger");
    CHECK_EQ(back.recipes["recipe.brain_burger"].row, 217);
    CHECK_EQ(back.recipes["recipe.brain_burger"].first_ts, 1001);
    CHECK_EQ(back.recipes["recipe.brain_burger"].last_ts, 1002);
    CHECK_EQ(back.recipes["row:7"].cooked, 1);
    CHECK(back.recipes["row:7"].zcr_id == "");
    CHECK(back.achievements["ach.cook_first"].unlocked);
    CHECK(back.achievements["ach.cook_first"].claimed);
    CHECK_EQ(back.achievements["ach.cook_first"].claimed_ts, 1006);

    /* double round-trip is stable */
    CHECK(serialize(back) == json);
}

static void test_state_parse_hardening() {
    State s;
    std::string err;
    CHECK(!parse_state("", &s, &err));
    CHECK(!parse_state("{", &s, &err));
    CHECK(!parse_state("[1,2]", &s, &err));
    CHECK(!parse_state("{} trailing", &s, &err));
    CHECK(!parse_state("{\"version\":1.5}", &s, &err));
    /* missing sections default sanely */
    CHECK(parse_state("{\"version\":1}", &s, &err));
    CHECK_EQ(s.total_cooked, 0);
    /* negative counts clamped + totals floor at recipe sum */
    CHECK(parse_state(
        "{\"version\":1,\"totals\":{\"cooked\":0},"
        "\"recipes\":{\"row:1\":{\"cooked\":5}},"
        "\"achievements\":{\"a\":{\"progress\":-3,\"unlocked\":0}}}",
        &s, &err));
    CHECK_EQ(s.recipes["row:1"].cooked, 5);
    CHECK_EQ(s.total_cooked, 5); /* raised to the per-recipe sum */
    CHECK_EQ(s.achievements["a"].progress, 0);
    /* negative recipe count clamped */
    CHECK(parse_state("{\"version\":1,\"recipes\":{\"row:2\":{\"cooked\":-9}}}",
                      &s, &err));
    CHECK_EQ(s.recipes["row:2"].cooked, 0);
}

static void test_defs_parse() {
    Defs d;
    std::string err;
    CHECK(!parse_defs("{\"achievements\":[{\"type\":\"cook_total\"}]}", &d, &err));
    CHECK(!parse_defs(
        "{\"achievements\":[{\"id\":\"a\",\"type\":\"bogus\"}]}", &d, &err));
    /* unknown fields tolerated, thresholds normalized */
    CHECK(parse_defs(
        "{\"mastery\":{\"levels\":[200,10,10,50]},"
        "\"achievements\":[{\"id\":\"a.b\",\"type\":\"cook_total\","
        "\"threshold\":3,\"reward_toxin\":-5,\"extra\":1}]}",
        &d, &err));
    CHECK_EQ(d.mastery_levels.size(), 3); /* unique + sorted */
    CHECK_EQ(d.mastery_levels[0], 10);
    CHECK_EQ(d.mastery_levels[2], 200);
    CHECK_EQ(d.achievements.size(), 1);
    CHECK_EQ(d.achievements[0].reward_toxin, 0);
    CHECK(d.achievements[0].type == AchType::CookTotal);
    /* missing mastery defaults */
    CHECK(parse_defs("{\"achievements\":[]}", &d, &err));
    CHECK_EQ(d.mastery_levels.size(), 3);
    CHECK(!parse_defs("not json", &d, &err));
}

static void test_registry_rows() {
    std::map<int32_t, std::string> rows;
    std::string err;
    const char *json =
        "{\"version\":1,\"entries\":["
        "{\"id\":\"char.man\",\"file\":\"characterData.bin.mid.json\",\"row\":0},"
        "{\"id\":\"recipe.brain_burger\",\"file\":\"foodData.bin.mid.json\",\"row\":217},"
        "{\"id\":\"recipe.burger\",\"file\":\"foodData.bin.mid.json\",\"row\":0},"
        "{\"id\":\"furniture.x\",\"file\":\"furnitureData.bin.mid.json\",\"row\":3}"
        "]}";
    CHECK(parse_registry_rows(json, "foodData.bin.mid.json", &rows, &err));
    CHECK_EQ(rows.size(), 2);
    CHECK(rows[0] == "recipe.burger");
    CHECK(rows[217] == "recipe.brain_burger");
    rows.clear();
    CHECK(parse_registry_rows(json, "characterData.bin.mid.json", &rows, &err));
    CHECK_EQ(rows.size(), 1);
    CHECK(rows[0] == "char.man");
    CHECK(!parse_registry_rows("{}", "foodData.bin.mid.json", &rows, &err));
    CHECK(!parse_registry_rows("nope", "foodData.bin.mid.json", &rows, &err));
}

static void test_json_escapes() {
    State s;
    s.version = 1;
    RecipeStat r;
    r.zcr_id = "recipe.\"quoted\"\\slash";
    r.row = 3;
    r.cooked = 1;
    s.recipes["row:3"] = r;
    std::string json = serialize(s);
    State back;
    std::string err;
    CHECK(parse_state(json, &back, &err));
    CHECK(back.recipes["row:3"].zcr_id == "recipe.\"quoted\"\\slash");
    /* \u escapes in input */
    State u;
    CHECK(parse_state("{\"version\":1,\"recipes\":{\"row:1\":{"
                      "\"zcr\":\"a\\u0041\\tb\"}}}",
                      &u, &err));
    CHECK(u.recipes["row:1"].zcr_id == "aA\tb");
}

static void test_favorites() {
    State s;
    set_favorite(s, "recipe.mystery_meat", 0, true, 100);
    set_favorite(s, "recipe.brain_burger", 217, true, 101);
    CHECK_EQ(s.favorites.size(), 2);
    CHECK_EQ(s.favorites["recipe.mystery_meat"], 0);
    CHECK_EQ(s.favorites["recipe.brain_burger"], 217);
    /* un-favorite erases */
    set_favorite(s, "recipe.mystery_meat", 0, false, 102);
    CHECK_EQ(s.favorites.size(), 1);
    CHECK(s.favorites.find("recipe.mystery_meat") == s.favorites.end());
    /* idempotent */
    set_favorite(s, "recipe.brain_burger", 217, true, 103);
    CHECK_EQ(s.favorites.size(), 1);

    /* round-trip */
    std::string json = serialize(s);
    CHECK(json.find("\"favorites\":{\"recipe.brain_burger\":217}") !=
          std::string::npos);
    State back;
    std::string err;
    CHECK(parse_state(json, &back, &err));
    CHECK_EQ(back.favorites.size(), 1);
    CHECK_EQ(back.favorites["recipe.brain_burger"], 217);
    CHECK(serialize(back) == json);

    /* parse hardening: non-int / out-of-range rows dropped */
    State u;
    CHECK(parse_state("{\"version\":1,\"favorites\":{"
                      "\"recipe.a\":5,\"recipe.b\":999,\"recipe.c\":-1,"
                      "\"recipe.d\":\"x\"}}",
                      &u, &err));
    CHECK_EQ(u.favorites.size(), 1);
    CHECK_EQ(u.favorites["recipe.a"], 5);

    /* engine cap respected by callers; state itself stores what it is given */
    State c;
    for (int i = 0; i < 20; i++) {
        char key[32];
        snprintf(key, sizeof(key), "recipe.r%d", i);
        set_favorite(c, key, i, true, 200 + i);
    }
    CHECK_EQ(c.favorites.size(), 20); /* storage is unlimited; the engine
                                       * list rebuild caps at 14 */
}

static void test_toxin_ledger() {
    Defs d = make_defs();
    State s;
    on_toxin_spent(s, d, 1, 100, "cook_finish");
    on_toxin_spent(s, d, 2, 101, "recipe_unlock");
    on_toxin_spent(s, d, 3, 102, "");
    CHECK_EQ(s.toxin_log.size(), 3);
    CHECK(s.toxin_log[0].flow == "cook_finish");
    CHECK_EQ(s.toxin_log[0].amount, 1);
    CHECK(s.toxin_log[1].flow == "recipe_unlock");
    CHECK(s.toxin_log[2].flow == "unknown"); /* empty -> unknown */
    /* zero/negative amounts do not log */
    on_toxin_spent(s, d, 0, 103, "x");
    on_toxin_spent(s, d, -5, 103, "x");
    CHECK_EQ(s.toxin_log.size(), 3);
    CHECK_EQ(s.toxin_spent, 6);

    /* round-trip */
    std::string json = serialize(s);
    State back;
    std::string err;
    CHECK(parse_state(json, &back, &err));
    CHECK_EQ(back.toxin_log.size(), 3);
    CHECK(back.toxin_log[0].flow == "cook_finish");
    CHECK_EQ(back.toxin_log[2].amount, 3);
    CHECK(serialize(back) == json);

    /* cap: keep the most recent 32 */
    State c;
    for (int i = 0; i < 40; i++) {
        char f[16];
        snprintf(f, sizeof(f), "flow%d", i);
        on_toxin_spent(c, d, 1, 200 + i, f);
    }
    CHECK_EQ(c.toxin_log.size(), kToxinLogCap);
    CHECK(c.toxin_log.front().flow == "flow8");   /* oldest 8 dropped */
    CHECK(c.toxin_log.back().flow == "flow39");

    /* parse: oversized file capped, junk entries dropped */
    State u;
    std::string big = "{\"version\":1,\"toxin_log\":[";
    for (int i = 0; i < 50; i++) {
        if (i) big += ",";
        char e[64];
        snprintf(e, sizeof(e), "{\"t\":%d,\"n\":%d,\"f\":\"f%d\"}", i, i + 1, i);
        big += e;
    }
    big += ",{\"t\":1,\"n\":0,\"f\":\"zero\"},{\"t\":1,\"f\":\"noN\"}]}";
    CHECK(parse_state(big, &u, &err));
    CHECK_EQ(u.toxin_log.size(), kToxinLogCap);
    CHECK(u.toxin_log.back().flow == "f49");
    CHECK(u.toxin_log.front().flow == "f18");
}

/* ---- seasonal events (zcr_ev) ---- */

static void test_event_calendars() {
    using namespace zcr_ev;
    int32_t m = 0, d = 0;
    easter_sunday(2024, &m, &d);
    CHECK_EQ(m, 3);
    CHECK_EQ(d, 31);
    easter_sunday(2025, &m, &d);
    CHECK_EQ(m, 4);
    CHECK_EQ(d, 20);
    easter_sunday(2026, &m, &d);
    CHECK_EQ(m, 4);
    CHECK_EQ(d, 5);

    CHECK_EQ(event_for_date(2026, 10, 15), (int)EvHalloween);
    CHECK_EQ(event_for_date(2026, 10, 31), (int)EvHalloween);
    CHECK_EQ(event_for_date(2026, 11, 5), (int)EvHalloween);
    CHECK_EQ(event_for_date(2026, 11, 6), (int)EvNone);
    CHECK_EQ(event_for_date(2026, 12, 1), (int)EvChristmas);
    CHECK_EQ(event_for_date(2026, 12, 25), (int)EvChristmas);
    CHECK_EQ(event_for_date(2027, 1, 5), (int)EvChristmas);
    CHECK_EQ(event_for_date(2027, 1, 6), (int)EvNone);
    /* Easter 2026 = Apr 5 -> window Mar 29 .. Apr 8 */
    CHECK_EQ(event_for_date(2026, 3, 28), (int)EvNone);
    CHECK_EQ(event_for_date(2026, 3, 29), (int)EvEaster);
    CHECK_EQ(event_for_date(2026, 4, 5), (int)EvEaster);
    CHECK_EQ(event_for_date(2026, 4, 8), (int)EvEaster);
    CHECK_EQ(event_for_date(2026, 4, 9), (int)EvNone);
}

static void test_event_roll() {
    using namespace zcr_ev;
    EventState a = make_event(EvChristmas, 2026, 1000);
    EventState b = make_event(EvChristmas, 2026, 2000);
    CHECK_EQ(a.objectives.size(), 3);
    for (int i = 0; i < 3; i++) {
        CHECK_EQ(a.objectives[i].kind, b.objectives[i].kind);
        CHECK_EQ(a.objectives[i].target, b.objectives[i].target);
    }
    CHECK(a.objectives[0].kind != a.objectives[1].kind);
    CHECK(a.objectives[1].kind != a.objectives[2].kind);
    CHECK(a.objectives[0].kind != a.objectives[2].kind);
    for (int i = 0; i < 3; i++) {
        const Objective &o = a.objectives[i];
        CHECK(o.target > 0);
        CHECK(o.reward_coins > 0);
        if (o.kind == ObjCoins) CHECK_EQ(o.reward_coins, o.target / 4);
        if (o.kind == ObjServe) CHECK_EQ(o.reward_coins, o.target * 60);
    }
    /* "randomize every halloween/easter/christmas": next year differs */
    EventState c = make_event(EvChristmas, 2027, 1000);
    bool differs = false;
    for (int i = 0; i < 3; i++)
        if (c.objectives[i].kind != a.objectives[i].kind ||
            c.objectives[i].target != a.objectives[i].target)
            differs = true;
    CHECK(differs);
}

static void test_event_progress() {
    using namespace zcr_ev;
    EventState s;
    s.event = EvHalloween;
    Objective o;
    o.kind = ObjServe;
    o.target = 10;
    o.reward_coins = 600;
    s.objectives.push_back(o);
    CHECK_EQ(add_progress(&s, ObjServe, 4).size(), 0);
    CHECK_EQ(s.objectives[0].progress, 4);
    CHECK_EQ(add_progress(&s, ObjServe, 6).size(), 1);
    CHECK(s.objectives[0].done);
    CHECK_EQ(s.objectives[0].progress, 10);
    /* done latches: no double completion, no overcount */
    CHECK_EQ(add_progress(&s, ObjServe, 50).size(), 0);
    CHECK_EQ(s.objectives[0].progress, 10);
    /* wrong kind ignored */
    CHECK_EQ(add_progress(&s, ObjCook, 5).size(), 0);
    /* over-target clamps */
    EventState s2;
    s2.event = EvChristmas;
    Objective o2;
    o2.kind = ObjToxin;
    o2.target = 10;
    o2.reward_coins = 1500;
    s2.objectives.push_back(o2);
    CHECK_EQ(add_progress(&s2, ObjToxin, 25).size(), 1);
    CHECK(s2.objectives[0].done);
    CHECK_EQ(s2.objectives[0].progress, 10);
    /* zero/negative amounts are inert */
    CHECK_EQ(add_progress(&s2, ObjToxin, 0).size(), 0);
}

static void test_event_roundtrip() {
    using namespace zcr_ev;
    EventState s = make_event(EvEaster, 2026, 123456);
    s.announced = true;
    s.objectives[1].progress = 3;
    std::string js = serialize(s);
    EventState t;
    CHECK(parse(js, &t));
    CHECK_EQ(t.event, s.event);
    CHECK_EQ(t.year, s.year);
    CHECK_EQ(t.started_ts, s.started_ts);
    CHECK(t.announced);
    CHECK_EQ(t.objectives.size(), s.objectives.size());
    for (size_t i = 0; i < s.objectives.size(); i++) {
        CHECK_EQ(t.objectives[i].kind, s.objectives[i].kind);
        CHECK_EQ(t.objectives[i].target, s.objectives[i].target);
        CHECK_EQ(t.objectives[i].progress, s.objectives[i].progress);
        CHECK_EQ(t.objectives[i].reward_coins, s.objectives[i].reward_coins);
        CHECK_EQ(t.objectives[i].done, s.objectives[i].done);
        CHECK_EQ(t.objectives[i].rewarded, s.objectives[i].rewarded);
    }
    EventState u;
    CHECK(!parse("{}", &u));
    CHECK(!parse("", &u));
    CHECK(!parse("{\"objectives\":[]", &u));
}

static void test_event_names_and_food() {
    using namespace zcr_ev;
    CHECK_EQ(event_from_name("christmas"), (int)EvChristmas);
    CHECK_EQ(event_from_name("christmas\n"), (int)EvChristmas);
    CHECK_EQ(event_from_name("HalloweEn"), (int)EvNone); /* case-sensitive */
    CHECK_EQ(event_from_name("off"), (int)EvNone);
    CHECK(is_event_food(EvHalloween, 37));
    CHECK(!is_event_food(EvHalloween, 38));
    CHECK(is_event_food(EvChristmas, 71));
    CHECK(is_event_food(EvEaster, 66));
    CHECK(std::string(event_food_label(EvHalloween, 37)) == "Smash-o-Lantern");
    CHECK(std::string(event_special_name(EvChristmas)) == "Frostbitten Zombie");
}

/* ---- raid bosses (zcr_boss) ---- */

static void test_boss_flow() {
    using namespace zcr_boss;
    CHECK_EQ(pick_boss(1, 100), 0); /* below every level gate */
    int32_t b = pick_boss(3, 100);
    CHECK(b != 0);
    const BossDef *d = boss_by_id(b);
    CHECK(d != nullptr);
    if (d) CHECK(d->level_req <= 3);
    CHECK_EQ(pick_boss(20, 100), pick_boss(20, 100)); /* deterministic */

    BossState s;
    s.boss_id = 1; /* The Meat Grinder: level 3, 150 HP, no abilities */
    CHECK(!sync_level(&s, 2, 1000));
    CHECK_EQ(s.phase, (int)Locked);
    CHECK(sync_level(&s, 3, 1000));
    CHECK_EQ(s.phase, (int)Available);
    CHECK(start(&s, 1000));
    CHECK_EQ(s.phase, (int)Active);
    CHECK_EQ(s.hp, 150);
    CHECK_EQ(apply_damage(&s, 20, 1, 1001), 20);
    CHECK_EQ(s.hp, 130);
    CHECK_EQ(apply_damage(&s, 200, 2, 1002), 130); /* overkill clamps */
    CHECK_EQ(s.hp, 0);
    CHECK_EQ(s.phase, (int)Defeated);
    CHECK(is_defeated(s));
    s.reward_granted = true;
    CHECK(!is_defeated(s)); /* reward latch: one payout only */
}

static void test_boss_abilities() {
    using namespace zcr_boss;
    /* Shield (Frost Queen id 4): absorbs the first 120 */
    BossState s;
    s.boss_id = 4;
    CHECK(start(&s, 0));
    CHECK_EQ(s.shield_left, 120);
    CHECK_EQ(apply_damage(&s, 50, 1, 0), 0);
    CHECK_EQ(s.shield_left, 70);
    CHECK_EQ(apply_damage(&s, 100, 1, 0), 30);
    CHECK_EQ(s.shield_left, 0);
    /* Rage (Pumpkin King id 3): below 50% HP damage is halved */
    BossState r;
    r.boss_id = 3;
    CHECK(start(&r, 0));
    CHECK_EQ(apply_damage(&r, 350, 1, 0), 350);
    CHECK_EQ(r.hp, 350);
    CHECK_EQ(apply_damage(&r, 100, 1, 0), 50);
    CHECK_EQ(r.hp, 300);
    /* Regen (Baron id 2): 3 HP per minute */
    BossState g;
    g.boss_id = 2;
    CHECK(start(&g, 0));
    apply_damage(&g, 100, 1, 0);
    CHECK_EQ(g.hp, 300);
    CHECK(!tick(&g, 120));
    CHECK_EQ(g.hp, 306);
    /* Expiry + Thief penalty */
    BossState t;
    t.boss_id = 3;
    CHECK(start(&t, 0));
    CHECK(tick(&t, 25 * 3600));
    CHECK_EQ(t.phase, (int)Expired);
    CHECK_EQ(thief_penalty(t), 2000);
}

static void test_boss_roundtrip() {
    using namespace zcr_boss;
    BossState s;
    s.boss_id = 5;
    CHECK(start(&s, 777));
    apply_damage(&s, 250, 3, 778);
    std::string js = serialize(s);
    BossState t;
    CHECK(parse(js, &t));
    CHECK_EQ(t.boss_id, 5);
    CHECK_EQ(t.phase, s.phase);
    CHECK_EQ(t.hp, s.hp);
    CHECK_EQ(t.shield_left, s.shield_left);
    CHECK_EQ(t.damage_dealt, s.damage_dealt);
    CHECK_EQ(t.deadline_ts, s.deadline_ts);
    BossState u;
    CHECK(!parse("{}", &u));
    CHECK(!parse("", &u));
}

int main() {
    test_mastery();
    test_recipe_key();
    test_cook_counting();
    test_distinct_and_mastery_achievements();
    test_toxin_events();
    test_claim_latch();
    test_state_roundtrip();
    test_state_parse_hardening();
    test_defs_parse();
    test_registry_rows();
    test_json_escapes();
    test_favorites();
    test_toxin_ledger();
    test_event_calendars();
    test_event_roll();
    test_event_progress();
    test_event_roundtrip();
    test_event_names_and_food();
    test_boss_flow();
    test_boss_abilities();
    test_boss_roundtrip();

    printf("companion tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
