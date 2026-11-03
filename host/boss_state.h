/* Raid bosses with special abilities (pure logic).
 *
 * Companion-layered boss encounters on top of the engine's raid/attack
 * hooks: a boss becomes challengeable at a player level (ZombieCafe+0x1b0),
 * the player damages it by playing (serving/cooking dishes and landing
 * raid attacks via CharacterInstance::dealtDamage), and its special
 * abilities (Regen/Shield/Rage/Thief/Swarm) modify the fight. No engine
 * dependencies - unit-testable off-device like companion_state.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zcr_boss {

enum AbilityType {
    AbilNone = 0,
    AbilRegen = 1,   /* heals <magnitude> HP per minute           */
    AbilShield = 2,  /* absorbs the first <magnitude> damage      */
    AbilRage = 3,    /* below 50% HP takes <magnitude>% less dmg  */
    AbilThief = 4,   /* on timeout steals <magnitude> coins       */
    AbilSwarm = 5,   /* heals <magnitude> HP every 10 dishes      */
};

enum Phase {
    Locked = 0,     /* player level too low   */
    Available = 1,  /* challengeable          */
    Active = 2,     /* fight in progress      */
    Defeated = 3,   /* beaten, reward granted */
    Expired = 4,    /* ran out of time        */
};

struct Ability {
    int32_t type = 0;
    int64_t magnitude = 0;
};

struct BossDef {
    int32_t id;
    const char *name;
    int32_t level_req;
    int64_t max_hp;
    int64_t reward_coins;
    int32_t reward_toxin;
    int32_t duration_hours;
    Ability abilities[3];
};

struct BossState {
    int32_t boss_id = 0;
    int32_t phase = 0;
    int64_t hp = 0;
    int64_t shield_left = 0;
    int64_t damage_dealt = 0;
    int64_t dishes = 0;
    int64_t started_ts = 0;
    int64_t deadline_ts = 0;
    int64_t last_regen_ts = 0;
    bool announced_available = false;
    bool announced_active = false;
    bool reward_granted = false;
    int64_t cooldown_until = 0;
};

/* roster (id-stable, append-only) */
const BossDef *boss_by_id(int32_t id);
int boss_count();
const BossDef *boss_at(int idx);

/* Deterministic "boss of the day" among bosses whose level_req <= level. */
int32_t pick_boss(int32_t player_level, int64_t day_number);

/* State machine: apply player level; returns true when the phase changed
 * in a way the caller should announce (available/active). */
bool sync_level(BossState *s, int32_t player_level, int64_t now);

/* Start the fight (only from Available). */
bool start(BossState *s, int64_t now);

/* Apply damage through the abilities; returns actual HP removed.
 * source: 1=dish served, 2=dish cooked, 3=raid attack. */
int64_t apply_damage(BossState *s, int64_t amount, int source, int64_t now);

/* Timer tick: regen/swarm upkeep + expiry. Returns true when the boss
 * just expired (Thief). */
bool tick(BossState *s, int64_t now);

/* True when HP reached 0 in the Active phase (caller grants rewards). */
bool is_defeated(const BossState &s);

/* Thief penalty on expiry (0 when the boss has no Thief ability). */
int64_t thief_penalty(const BossState &s);

/* JSON (flat, self-contained) */
std::string serialize(const BossState &s);
bool parse(const std::string &json, BossState *out);

}  // namespace zcr_boss
