#include "event_state.h"

#include <cstdlib>
#include <ctime>

namespace zcr_ev {

void easter_sunday(int year, int32_t *month, int32_t *day) {
    int a = year % 19, b = year / 100, c = year % 100;
    int d = b / 4, e = b % 4;
    int f = (b + 8) / 25, g = (b - f + 1) / 3;
    int h = (19 * a + b - d - g + 15) % 30;
    int i = c / 4, k = c % 4;
    int l = (32 + 2 * e + 2 * i - h - k) % 7;
    int m = (a + 11 * h + 22 * l) / 451;
    *month = (h + l - 7 * m + 114) / 31;
    *day = ((h + l - 7 * m + 114) % 31) + 1;
}

namespace {

bool in_span(int month, int day, int from_m, int from_d, int to_m, int to_d) {
    int v = month * 100 + day;
    return v >= from_m * 100 + from_d && v <= to_m * 100 + to_d;
}

int day_of_year(int year, int month, int day) {
    static const int kCum[12] = {0, 31, 59, 90, 120, 151,
                                 181, 212, 243, 273, 304, 334};
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int doy = kCum[month - 1] + day;
    if (leap && month > 2) doy++;
    return doy;
}

uint32_t lcg_next(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return (*state >> 8) & 0xffffffu;
}

}  // namespace

EventId event_for_date(int year, int32_t month, int32_t day) {
    if (in_span(month, day, 12, 1, 12, 31)) return EvChristmas;
    if (in_span(month, day, 1, 1, 1, 5)) return EvChristmas;  /* tail */
    if (in_span(month, day, 10, 15, 11, 5)) return EvHalloween;
    int32_t em = 0, ed = 0;
    easter_sunday(year, &em, &ed);
    /* Easter-7d .. Easter+3d */
    int doy = day_of_year(year, month, day);
    int edoy = day_of_year(year, em, ed);
    if (doy >= edoy - 7 && doy <= edoy + 3) return EvEaster;
    return EvNone;
}

EventId event_for_time(int64_t unix_ts) {
    time_t t = (time_t)unix_ts;
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    return event_for_date(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}

EventId event_from_name(const std::string &raw) {
    std::string name = raw;
    while (!name.empty() && (name.back() == '\n' || name.back() == '\r' ||
                             name.back() == ' ' || name.back() == '\t'))
        name.pop_back();
    if (name == "halloween") return EvHalloween;
    if (name == "christmas") return EvChristmas;
    if (name == "easter") return EvEaster;
    return EvNone;
}

const char *event_name(int32_t id) {
    switch (id) {
        case EvHalloween: return "Halloween";
        case EvChristmas: return "Christmas";
        case EvEaster: return "Easter";
        default: return "None";
    }
}

namespace {

struct Pool {
    int32_t kind;
    int64_t targets[3];
    int64_t reward_per_unit;
};

const Pool kPools[5] = {
    {ObjServe, {25, 40, 60}, 60},
    {ObjCook, {20, 35, 50}, 60},
    {ObjCookEvent, {4, 8, 12}, 250},
    {ObjCoins, {5000, 12000, 25000}, 0},
    {ObjToxin, {10, 25, 50}, 150},
};

}  // namespace

EventState make_event(int32_t id, int32_t year, int64_t now) {
    EventState s;
    s.event = id;
    s.year = year;
    s.started_ts = now;
    s.announced = false;
    if (id == EvNone) return s;
    uint32_t rng = (uint32_t)(year * 1009 + id * 7717 + 13);
    /* pick 3 distinct pool entries */
    int order[5] = {0, 1, 2, 3, 4};
    for (int i = 4; i > 0; i--) {
        int j = (int)(lcg_next(&rng) % (uint32_t)(i + 1));
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
    for (int i = 0; i < 3; i++) {
        const Pool &p = kPools[order[i]];
        Objective o;
        o.kind = p.kind;
        int ti = (int)(lcg_next(&rng) % 3u);
        o.target = p.targets[ti];
        o.progress = 0;
        o.reward_coins = (p.kind == ObjCoins) ? o.target / 4
                                              : o.target * p.reward_per_unit;
        o.done = false;
        o.rewarded = false;
        s.objectives.push_back(o);
    }
    return s;
}

std::vector<int> add_progress(EventState *s, int32_t kind, int64_t amount) {
    std::vector<int> hit;
    if (!s || amount <= 0) return hit;
    for (size_t i = 0; i < s->objectives.size(); i++) {
        Objective &o = s->objectives[i];
        if (o.kind != kind || o.done) continue;
        o.progress += amount;
        if (o.progress >= o.target) {
            o.progress = o.target;
            o.done = true;
            hit.push_back((int)i);
        }
    }
    return hit;
}

/* ---- event-specialty recipes (foodData rows) ---- */

namespace {

struct FoodRow {
    int32_t row;
    const char *label;
};

/* Chosen from the shipped recipe set (docs/ASSET_EXPANSION_GUIDE). */
const FoodRow kHalloweenFood[] = {
    {37, "Smash-o-Lantern"}, {49, "Exjack-o-Lantern"}, {120, "Heart Dog on a Stake"},
    {125, "Crucy Fries"}, {123, "Brain Staker"}, {142, "Hot Frog"},
};
const FoodRow kChristmasFood[] = {
    {71, "Puking Duck"}, {70, "Fruit Cake"}, {64, "Tur-Key"},
    {63, "Caramel Adam's Apple"}, {131, "Super Fruit Punch"}, {130, "Zap Bar"},
};
const FoodRow kEasterFood[] = {
    {66, "Snot Cross Buns"}, {19, "Green Eggs and Sam"}, {46, "Zomelet"},
    {32, "Muscle Sprouts"}, {22, "Guc-a-mole"}, {67, "Bloodshake"},
};

const FoodRow *food_list(int32_t id, int *n) {
    switch (id) {
        case EvHalloween: *n = (int)(sizeof(kHalloweenFood) / sizeof(FoodRow));
            return kHalloweenFood;
        case EvChristmas: *n = (int)(sizeof(kChristmasFood) / sizeof(FoodRow));
            return kChristmasFood;
        case EvEaster: *n = (int)(sizeof(kEasterFood) / sizeof(FoodRow));
            return kEasterFood;
        default: *n = 0; return nullptr;
    }
}

}  // namespace

bool is_event_food(int32_t id, int32_t row) {
    int n = 0;
    const FoodRow *l = food_list(id, &n);
    for (int i = 0; i < n; i++)
        if (l[i].row == row) return true;
    return false;
}

const char *event_food_label(int32_t id, int32_t row) {
    int n = 0;
    const FoodRow *l = food_list(id, &n);
    for (int i = 0; i < n; i++)
        if (l[i].row == row) return l[i].label;
    return "";
}

const char *event_special_name(int32_t id) {
    switch (id) {
        case EvHalloween: return "Headless Barista Zombie";
        case EvChristmas: return "Frostbitten Zombie";
        case EvEaster: return "Zombie Bunny";
        default: return "";
    }
}

int32_t event_special_row(int32_t id) {
    /* characterData row for the event special. -1 = the roster/spawn RE
     * that pins this character into the cafe during the event is not
     * done yet (docs: CONTENT_EXPANSION_TEST_REPORT 14). */
    (void)id;
    return -1;
}

/* ---- JSON ---- */

std::string serialize(const EventState &s) {
    std::string out = "{\"event\":" + std::to_string(s.event) +
                      ",\"year\":" + std::to_string(s.year) +
                      ",\"started\":" + std::to_string(s.started_ts) +
                      ",\"announced\":" + (s.announced ? "1" : "0") +
                      ",\"objectives\":[";
    for (size_t i = 0; i < s.objectives.size(); i++) {
        const Objective &o = s.objectives[i];
        if (i) out += ",";
        out += "{\"k\":" + std::to_string(o.kind) +
               ",\"t\":" + std::to_string(o.target) +
               ",\"p\":" + std::to_string(o.progress) +
               ",\"c\":" + std::to_string(o.reward_coins) +
               ",\"d\":" + (o.done ? "1" : "0") +
               ",\"r\":" + (o.rewarded ? "1" : "0") + "}";
    }
    out += "]}";
    return out;
}

namespace {

bool find_int(const std::string &s, const char *key, int64_t *out) {
    std::string pat = std::string("\"") + key + "\":";
    size_t p = s.find(pat);
    if (p == std::string::npos) return false;
    p += pat.size();
    char *end = nullptr;
    long long v = strtoll(s.c_str() + p, &end, 10);
    if (end == s.c_str() + p) return false;
    *out = (int64_t)v;
    return true;
}

}  // namespace

bool parse(const std::string &json, EventState *out) {
    if (!out) return false;
    EventState s;
    int64_t v = 0;
    if (!find_int(json, "event", &v)) return false;
    s.event = (int32_t)v;
    if (find_int(json, "year", &v)) s.year = (int32_t)v;
    if (find_int(json, "started", &v)) s.started_ts = v;
    if (find_int(json, "announced", &v)) s.announced = (v != 0);

    size_t arr = json.find("\"objectives\":[");
    if (arr != std::string::npos) {
        size_t pos = json.find('{', arr);
        while (pos != std::string::npos && pos < json.size()) {
            size_t end = json.find('}', pos);
            if (end == std::string::npos) break;
            std::string obj = json.substr(pos, end - pos + 1);
            Objective o;
            if (!find_int(obj, "k", &v)) break;
            o.kind = (int32_t)v;
            find_int(obj, "t", &o.target);
            find_int(obj, "p", &o.progress);
            find_int(obj, "c", &o.reward_coins);
            if (find_int(obj, "d", &v)) o.done = (v != 0);
            if (find_int(obj, "r", &v)) o.rewarded = (v != 0);
            s.objectives.push_back(o);
            pos = json.find('{', end);
        }
    }
    *out = s;
    return true;
}

}  // namespace zcr_ev
