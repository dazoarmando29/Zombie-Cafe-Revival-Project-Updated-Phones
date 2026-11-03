#include "boss_state.h"

#include <cstdlib>
#include <cstring>

namespace zcr_boss {
namespace {

const BossDef kRoster[] = {
    {1, "The Meat Grinder", 3, 150, 3000, 5, 24,
     {{AbilNone, 0}, {AbilNone, 0}, {AbilNone, 0}}},
    {2, "Baron Von Brain", 6, 400, 8000, 10, 24,
     {{AbilRegen, 3}, {AbilNone, 0}, {AbilNone, 0}}},
    {3, "Pumpkin King", 8, 700, 14000, 15, 24,
     {{AbilRage, 50}, {AbilThief, 2000}, {AbilNone, 0}}},
    {4, "Frost Queen", 10, 900, 18000, 20, 24,
     {{AbilShield, 120}, {AbilRegen, 4}, {AbilNone, 0}}},
    {5, "The Lich of Leftovers", 14, 1500, 30000, 30, 36,
     {{AbilRegen, 5}, {AbilRage, 40}, {AbilShield, 180}}},
    {6, "Apocalypse Cow", 20, 3000, 60000, 50, 48,
     {{AbilShield, 300}, {AbilSwarm, 25}, {AbilThief, 8000}}},
};

constexpr int kRosterCount = (int)(sizeof(kRoster) / sizeof(BossDef));

const Ability *find_ability(const BossDef &d, int32_t type) {
    for (int i = 0; i < 3; i++)
        if (d.abilities[i].type == type) return &d.abilities[i];
    return nullptr;
}

uint32_t lcg(uint32_t *st) {
    *st = *st * 1664525u + 1013904223u;
    return (*st >> 8) & 0xffffffu;
}

}  // namespace

const BossDef *boss_by_id(int32_t id) {
    for (int i = 0; i < kRosterCount; i++)
        if (kRoster[i].id == id) return &kRoster[i];
    return nullptr;
}

int boss_count() { return kRosterCount; }

const BossDef *boss_at(int idx) {
    return (idx >= 0 && idx < kRosterCount) ? &kRoster[idx] : nullptr;
}

int32_t pick_boss(int32_t player_level, int64_t day_number) {
    int32_t pool[kRosterCount];
    int n = 0;
    for (int i = 0; i < kRosterCount; i++)
        if (kRoster[i].level_req <= player_level) pool[n++] = kRoster[i].id;
    if (n == 0) return 0;
    uint32_t st = (uint32_t)(day_number * 2654435761u + 17u);
    return pool[lcg(&st) % (uint32_t)n];
}

bool sync_level(BossState *s, int32_t player_level, int64_t now) {
    if (!s) return false;
    const BossDef *d = boss_by_id(s->boss_id);
    if (!d) return false;
    int32_t prev = s->phase;
    if (s->phase == Locked && player_level >= d->level_req) {
        if (now >= s->cooldown_until) s->phase = Available;
    } else if (s->phase == Locked && player_level < d->level_req) {
        s->phase = Locked;
    }
    return s->phase != prev && (s->phase == Available || s->phase == Active);
}

bool start(BossState *s, int64_t now) {
    if (!s || s->phase != Available) return false;
    const BossDef *d = boss_by_id(s->boss_id);
    if (!d) return false;
    s->phase = Active;
    s->hp = d->max_hp;
    const Ability *sh = find_ability(*d, AbilShield);
    s->shield_left = sh ? sh->magnitude : 0;
    s->damage_dealt = 0;
    s->dishes = 0;
    s->started_ts = now;
    s->deadline_ts = now + (int64_t)d->duration_hours * 3600;
    s->last_regen_ts = now;
    s->announced_active = true;
    s->reward_granted = false;
    return true;
}

int64_t apply_damage(BossState *s, int64_t amount, int source, int64_t now) {
    (void)now;
    if (!s || s->phase != Active || amount <= 0) return 0;
    const BossDef *d = boss_by_id(s->boss_id);
    if (!d) return 0;
    if (source == 1) s->dishes++;
    int64_t dmg = amount;
    /* Shield: absorbs the first N damage */
    if (s->shield_left > 0) {
        int64_t absorbed = dmg < s->shield_left ? dmg : s->shield_left;
        s->shield_left -= absorbed;
        dmg -= absorbed;
    }
    /* Rage: below 50% HP the boss takes reduced damage */
    if (dmg > 0 && s->hp * 2 <= d->max_hp) {
        const Ability *rg = find_ability(*d, AbilRage);
        if (rg && rg->magnitude > 0 && rg->magnitude < 100)
            dmg -= dmg * rg->magnitude / 100;
    }
    if (dmg <= 0) return 0;
    if (dmg > s->hp) dmg = s->hp;
    s->hp -= dmg;
    s->damage_dealt += dmg;
    if (s->hp == 0 && s->phase == Active) s->phase = Defeated;
    return dmg;
}

bool tick(BossState *s, int64_t now) {
    if (!s || s->phase != Active) return false;
    const BossDef *d = boss_by_id(s->boss_id);
    if (!d) return false;
    if (now >= s->deadline_ts) {
        s->phase = Expired;
        return true;
    }
    /* Regen: heal N HP per minute */
    const Ability *rg = find_ability(*d, AbilRegen);
    if (rg && rg->magnitude > 0 && s->hp < d->max_hp &&
        now - s->last_regen_ts >= 60) {
        int64_t mins = (now - s->last_regen_ts) / 60;
        s->last_regen_ts += mins * 60;
        s->hp += rg->magnitude * mins;
        if (s->hp > d->max_hp) s->hp = d->max_hp;
    }
    /* Swarm: heals every 10 dishes served */
    const Ability *sw = find_ability(*d, AbilSwarm);
    if (sw && sw->magnitude > 0 && s->dishes > 0 && s->dishes % 10 == 0 &&
        s->hp < d->max_hp) {
        s->hp += sw->magnitude;
        if (s->hp > d->max_hp) s->hp = d->max_hp;
    }
    return false;
}

bool is_defeated(const BossState &s) {
    return s.phase == Defeated && !s.reward_granted;
}

int64_t thief_penalty(const BossState &s) {
    const BossDef *d = boss_by_id(s.boss_id);
    if (!d) return 0;
    const Ability *th = find_ability(*d, AbilThief);
    return th ? th->magnitude : 0;
}

std::string serialize(const BossState &s) {
    std::string out = "{\"boss\":" + std::to_string(s.boss_id) +
                      ",\"phase\":" + std::to_string(s.phase) +
                      ",\"hp\":" + std::to_string(s.hp) +
                      ",\"shield\":" + std::to_string(s.shield_left) +
                      ",\"dealt\":" + std::to_string(s.damage_dealt) +
                      ",\"dishes\":" + std::to_string(s.dishes) +
                      ",\"started\":" + std::to_string(s.started_ts) +
                      ",\"deadline\":" + std::to_string(s.deadline_ts) +
                      ",\"regen\":" + std::to_string(s.last_regen_ts) +
                      ",\"annA\":" + (s.announced_available ? "1" : "0") +
                      ",\"annB\":" + (s.announced_active ? "1" : "0") +
                      ",\"rewarded\":" + (s.reward_granted ? "1" : "0") +
                      ",\"cooldown\":" + std::to_string(s.cooldown_until) +
                      "}";
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

bool parse(const std::string &json, BossState *out) {
    if (!out) return false;
    BossState s;
    int64_t v = 0;
    if (!find_int(json, "boss", &v)) return false;
    s.boss_id = (int32_t)v;
    find_int(json, "phase", &v);
    s.phase = (int32_t)v;
    find_int(json, "hp", &s.hp);
    find_int(json, "shield", &s.shield_left);
    find_int(json, "dealt", &s.damage_dealt);
    find_int(json, "dishes", &s.dishes);
    find_int(json, "started", &s.started_ts);
    find_int(json, "deadline", &s.deadline_ts);
    find_int(json, "regen", &s.last_regen_ts);
    if (find_int(json, "annA", &v)) s.announced_available = (v != 0);
    if (find_int(json, "annB", &v)) s.announced_active = (v != 0);
    if (find_int(json, "rewarded", &v)) s.reward_granted = (v != 0);
    find_int(json, "cooldown", &s.cooldown_until);
    *out = s;
    return true;
}

}  // namespace zcr_boss
