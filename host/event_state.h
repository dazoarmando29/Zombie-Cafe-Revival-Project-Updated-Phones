/* Seasonal game events (Halloween / Christmas / Easter): pure logic.
 *
 * No engine or emulator dependencies - unit-testable off-device like
 * companion_state. The host layer (zcr_companion.cpp) drives it from
 * engine hooks and persists it to zcr_events.json.
 *
 * Events are date-window based (with a live override file for testing),
 * and their objectives are randomized deterministically per year so
 * every Halloween/Easter/Christmas brings a different set of tasks.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zcr_ev {

enum EventId {
    EvNone = 0,
    EvHalloween = 1,
    EvChristmas = 2,
    EvEaster = 3,
};

enum ObjKind {
    ObjServe = 1,      /* dishes placed on serving spots        */
    ObjCook = 2,       /* dishes carried to the counter (cooked)*/
    ObjCookEvent = 3,  /* event-specialty dishes cooked         */
    ObjCoins = 4,      /* coins earned                          */
    ObjToxin = 5,      /* toxin spent                           */
};

struct Objective {
    int32_t kind = 0;
    int64_t target = 0;
    int64_t progress = 0;
    int64_t reward_coins = 0;
    bool done = false;
    bool rewarded = false;
};

struct EventState {
    int32_t event = 0;      /* EventId of the active/last event */
    int32_t year = 0;       /* the year the event was rolled    */
    int64_t started_ts = 0;
    bool announced = false;
    std::vector<Objective> objectives;
};

/* ---- calendars ---- */

/* Easter Sunday (Gregorian) for a year; writes month (3/4) and day. */
void easter_sunday(int year, int32_t *month, int32_t *day);

/* Season windows by (year, month 1-12, day 1-31), local time:
 *   Halloween: Oct 15 .. Nov 5
 *   Christmas: Dec 1 .. Jan 5
 *   Easter:    Easter-7d .. Easter+3d
 * Returns EvNone outside every window. */
EventId event_for_date(int year, int32_t month, int32_t day);

/* Convenience: from a unix timestamp via localtime. */
EventId event_for_time(int64_t unix_ts);

/* Override file names: "halloween" / "christmas" / "easter" / "off". */
EventId event_from_name(const std::string &name);
const char *event_name(int32_t id);

/* ---- event instance ---- */

/* Deterministic per-year objectives (seed = year + event id): three
 * distinct kinds from the pools in event_state.cpp. */
EventState make_event(int32_t id, int32_t year, int64_t now);

/* Adds progress; returns the indices of objectives that completed with
 * this call (each index reported exactly once - done latches). */
std::vector<int> add_progress(EventState *s, int32_t kind, int64_t amount);

/* Event-specialty recipes: true when the foodData row is a designated
 * specialty of that event (CookEventFood objective). */
bool is_event_food(int32_t id, int32_t row);
/* Human label for notifications, e.g. "Smash-o-Lantern"; "" when unknown. */
const char *event_food_label(int32_t id, int32_t row);

/* Designated "special zombie" of the event (characterData row; -1 while
 * the roster/spawn RE is pending - the name is what the event announces). */
const char *event_special_name(int32_t id);
int32_t event_special_row(int32_t id);

/* ---- JSON (self-contained, flat) ---- */

std::string serialize(const EventState &s);
bool parse(const std::string &json, EventState *out);

}  // namespace zcr_ev
