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

    printf("companion tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
