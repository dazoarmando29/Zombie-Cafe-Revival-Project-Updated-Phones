/* Recipe mastery + achievements companion layer (host side).
 *
 * Listens to verified engine transitions (disassembly in
 * docs/ASSET_EXPANSION_GUIDE.md §5) and feeds the pure-logic companion
 * state (companion_state.h):
 *
 *   DishToCounterTask::completeObjective  -> one cooked dish (the engine's
 *       own "you cooked this" moment; it feeds ObjectiveSystem::cookFood
 *       from the very same call - see the RE notes)
 *   ZombieCafe::spendToxin(int)           -> toxin spent (achievements +
 *       foundation for the toxin-spend confirmation QoL)
 *   ZombieCafe::addToxin(int,bool)        -> gameplay toxin earned
 *       (flag==1 = purchase flow, excluded)
 *
 * The companion save is a versioned JSON file next to the engine saves
 * (internal files dir). It never touches the engine save format. It is
 * exported through SaveGuard (Download/ZombieCafeRevival survives app
 * uninstall) and restored through SaveGuard.restoreInternal where the OS
 * allows reading it back.
 */
#pragma once

#include <cstdint>
#include <string>

struct uc_struct;
typedef struct uc_struct uc_engine;

namespace zcr_companion {

/* Pre-scan assets (zcr_achievements.json, data/zcr_registry.lock.json),
 * load/restore the companion save and register the engine hooks. Call
 * once from host_init after jni_bridge_init. */
bool init(uc_engine *uc, void *asset_mgr);

/* Persist now (e.g. on app pause). Safe to call from any thread. */
void flush(bool force);

/* QoL driver tick: auto-cook idle stoves (repeat last recipe; notify the
 * user when coins are short). Throttled internally. MUST run on the
 * emulator thread (nested engine calls). */
void qol_tick();

/* Seasonal events: id of the active event for the weather overlay
 * (0=off, 1=halloween, 2=christmas, 3=easter). Safe from any thread. */
int weather_event();

/* Pop the pending event announcement/progress toast (once). Safe from
 * any thread. */
bool take_toast(char *buf, size_t n);
std::string zombie_perks_text();
int32_t perk_level_of(uint32_t char_data);
std::string boss_status_text();
std::string staff_name_of(uint32_t instance);

}  // namespace zcr_companion
