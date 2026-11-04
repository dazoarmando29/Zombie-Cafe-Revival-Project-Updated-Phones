# Zombie Cafe — Seasonal Expansion (Design Project)

Complete visual design project for a **Zombie Cafe** seasonal expansion, in the spirit of
the 2011 Beeline Interactive mobile game: 12 new zombie characters, three seasonal event
themes (Halloween, Christmas, Easter), event shops, recipes, redesigned game menus and an
interactive browser prototype.

This is a **visual design + interactive prototype** — it does not modify the game itself.

## Run the prototype

```powershell
npm install
npm run dev
```

Then open **http://localhost:5178** in a browser.

Production build: `npm run build` (output in `dist/`), preview with `npm run preview`.

## What you can do in the prototype

- **Event Selector** — switch between Original Cafe / Halloween / Christmas / Easter;
  every screen re-skins instantly via the seasonal theme system.
- **Event Hub** — event story, live countdown to the next event window, challenge
  tracker and reward previews.
- **Zombiepedia** — per-season collection page with progress bar, locked/unlocked
  states and character detail popups (rarity, role, height, flavor text).
- **All Zombies** — the full 12-character collection gallery.
- **Event Shop** — seasonal staff, furniture, decorations and kitchen equipment with
  category switching, prices and availability labels.
- **Recipes** — 12 illustrated seasonal recipes with course, cook time and ingredients.
- **Menu Preview** — all 8 game menus (Main Menu, Zombiepedia, Zombie/Furniture/
  Decoration Shop, Recipe Selection, Event Selection, Reward Screen) rendered in a
  phone frame for the active season.
- **Design Compare** — any menu screen side-by-side: Original Cafe vs each seasonal
  version.

## Artwork & exportable assets

All character/item art is **hand-built SVG source** (no emoji, no placeholder art):
editable components in `src/art/characters/` (12 zombies), `src/art/items.tsx`
(18 furniture/decor items + 12 recipe dishes) and `src/art/ui.tsx` (icons).

Export every design as standalone SVG files:

```powershell
npm run export-assets
```

This writes **70 real SVG files** to `public/assets/`:

```
public/assets/
├── zombies/
│   ├── halloween/   pumpkin-head, haunted-bride, scarecrow, vampire-chef (+ portraits/)
│   ├── christmas/   zombie-santa, frozen-elf, gingerbread, reindeer-zombie (+ portraits/)
│   └── easter/      rotten-bunny, egghead, zombie-chick, chocolate-bunny (+ portraits/)
├── furniture/
│   ├── halloween/   haunted-table, pumpkin-chair, coffin-couch, spooky-stove, ...
│   ├── christmas/   holiday-table, snow-chair, christmas-tree, present-stack, ...
│   └── easter/      egg-table, bunny-chair, easter-basket, spring-fountain, ...
├── recipes/         12 seasonal dishes (witch-brew-stew, snowman-stew, ...)
├── sheets/          halloween/christmas/easter collection sheets (4-up character art)
├── backgrounds/     themed cafe backgrounds (800x450)
└── ui/              icon-pumpkin, icon-snowflake, icon-egg, icon-ghost, icon-star, ...
```

- Full-body characters are 200x260, portraits 120x120 (head crop) — consistent dimensions.
- PNG conversion: any SVG can be rasterized (the QA script `scripts/rasterize-qa.mjs`
  uses `@resvg/resvg-js` and is how the character art was visually reviewed).
- **Menu screenshots** are not exported as raster files — capture them from the running
  prototype (`npm run dev` → Menu Preview tab) or use the `backgrounds/` + `ui/` assets
  to rebuild the screens.

## Quality checks

```powershell
npm run build          # TypeScript + Vite production build
node scripts/smoke-render.mjs   # server-renders every view/menu, asserts content
node scripts/rasterize-qa.mjs   # rasterizes character art for visual review
```

## Design system

- **Style** — classic 2D cartoon mobile-game art: large heads, thick ink outlines
  (`#2A1C10`), flat muted fills, chunky 3D buttons, wooden signboard panels.
- **Palettes**
  - Original Cafe: warm wood `#8C5A2B`, cream `#F6E7C1`, red `#E8542F`, gold `#F2B632`
  - Halloween: dark purple `#2B1B4A`, pumpkin orange `#F4801E`, toxic green `#9BE23B`
  - Christmas: red `#B32330`, evergreen `#1E6B3A`, gold `#E8B84B`, icy blue `#A8D8E8`
  - Easter: pastel yellow `#F6E58B`, mint `#A8E6C9`, lavender `#C9B6E4`, pink `#F5B7C6`
- **Type** — Luckiest Guy (display) / Baloo 2 (body), outlined "sticker" titles.
- Themes are CSS variables under `[data-event=...]` (`src/styles/theme.css`), so every
  component re-skins automatically per season.

## Content inventory

| | Halloween | Christmas | Easter |
|---|---|---|---|
| Zombies (4 each) | Pumpkin Head, Haunted Bride, Scarecrow, Vampire Chef | Zombie Santa, Frozen Elf, Gingerbread Zombie, Reindeer Zombie | Rotten Easter Bunny, Egghead, Zombie Chick, Chocolate Bunny |
| Rarities | Rare/Rare/Uncommon/Legendary | Legendary/Uncommon/Common/Rare | Uncommon/Common/Common/Rare |
| Furniture/decor (6 each) | Haunted Table, Pumpkin Chair, Coffin Couch, Spooky Stove, Ghost Chandelier, Haunted Wallpaper | Holiday Table, Snow Chair, Christmas Tree, Present Stack, Festive Stove, Snowy Wallpaper | Egg Table, Bunny Chair, Easter Basket, Spring Fountain, Egg Stove, Spring Wallpaper |
| Recipes (4 each) | Witch's Brew Stew, Severed Finger Fries, Eyeball Punch, Graveyard Cake | Snowman Stew, Ginger Ghoul Cookies, Candy Cane Cranium, Yule Slime Log | Rotten Egg Omelette, Bunny Tail Truffles, Jelly Bean Stew, Hot Cross Brains |

Event windows: Halloween **Oct 15 – Nov 5**, Christmas **Dec 10 – Jan 5**,
Easter **Mar 20 – Apr 15** (countdown timers in the prototype compute the next start).

---

Original fan design concept. Not affiliated with Beeline Interactive or Capcom.
