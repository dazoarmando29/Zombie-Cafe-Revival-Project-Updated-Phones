# Gameplay Balance — Zombie Café Revival

Audit of the shipped economy plus rules for new content. Numbers computed
from `src/assets/data/*.bin.mid.json` (2026-11-01, 217 recipes).

## 1. Recipe economy (shipped data)

| Metric | Min | Median | Max |
|---|---|---|---|
| Profit per hour (servings × price/serving ÷ cook hours) | 40 | 225 | 3600 |
| XP per hour | 2.7 | 10.7 | 220 |

* Profit/hour varies ~90× across recipes; the very high end (3600/h) is a
  small number of short high-yield recipes that the original game used as
  premium/limited items. Long-cook recipes deliberately trade rate for
  offline-friendly batches.
* XP/hour spread (2.7–220) follows the same shape.

**Assessment**: the spread is intentional (idle-game time-shifting) and
was shipped this way; no retroactive rebalance is planned. The classic
balance remains the default.

## 2. New-content balance rules

* Place each new recipe inside the profit/XP band of its unlock level
  (±30% of the band median). Example: `recipe.brain_burger` (level 8)
  lands at 180 profit/h and 8.6 XP/h versus a level-8 band of
  133–420 / 4.5–25.
* New characters: stat multipliers (`CookSpeedBonus`, `TipMultiplier`,
  `RegenBoost`, `CookXPBonus`) within the range of same-tier characters;
  rare characters may exceed common ones modestly, never universally.
* New furniture: keep `MAX_RATING_BONUS`/earning contributions within
  existing item tiers.
* Mastery/achievement rewards (implemented 2026-11-02, host companion
  layer): mastery tiers at 10/50/200 cooks per recipe (levels 1/2/3,
  cosmetic progression); achievement toxin rewards bounded at 1/2/3/5
  per achievement, ~31 toxin across the full 15-achievement set,
  non-repeatable. Nothing that permanently trivializes the economy; no
  infinitely farmable toxin source. Definitions live in
  `src/assets/data/zcr_achievements.json` (data-driven, tune in place).

## 3. Toxin economy

* Sources today: initial grant, level/objective rewards, purchases (the
  buy patch maps money-buy dialogs to toxin-buy), occasional boss/quest
  payouts (online quests patched to offline objectives).
* `ZombieCafe_MAX_AWARDED_TOXIN_PER_MONTH` exists in the engine constants
  and bounds award flows.
* Planned additions (achievements, mastery, quests, bosses) must respect
  the monthly-award cap design and stay non-repeatable.

## 4. Progression bottlenecks (observed)

* FoodData rows show deliberate pacing curves; the largest grind walls in
  the original are the high-cook-time recipes (4000+ minutes) and staff
  infection costs. Casual-mode options (food expiration grace, faster
  early cooking) are planned as **opt-in** settings; the classic curve
  stays the default and existing saves keep their difficulty.

## 5. Process

Every content PR should include a balance note (band comparison) in its
description; the registry lock guarantees rows can be tuned in place but
never silently moved.
