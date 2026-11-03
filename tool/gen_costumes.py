#!/usr/bin/env python3
"""Generate costume variants for zombies (male + female) with accessories.

For each costume definition below this tool:
  1. copies a base part folder (characterParts*/<base>-zombie|-human) into
     characterParts5/<art>-zombie and <art>-human with palette-rotated
     PNGs (sidecar .png.json geometry files are copied verbatim),
  2. appends character rows to src/assets/data/characterData.bin.mid.json
     (everything under ZcrId "char.costume." is regenerated on re-run),
     reusing the base row's fields and overriding Name / art strings /
     IsFemale / CafeLevelRequired / stats / cost.

The N-sheet pipeline (tool/build_tool) packs characterParts5/ like any
other sheet, and the engine resolves the art string to the folder
("art-string-to-folder resolution" - see the asset validator).

Idempotent: delete rows/folders by name and re-run.
"""
import json
import os
import shutil
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = os.path.join(ROOT, "src", "assets", "images")
CHARS_JSON = os.path.join(ROOT, "src", "assets", "data",
                          "characterData.bin.mid.json")
OUT_SHEET = os.path.join(IMAGES, "characterParts5")

# hue rotation (deg), saturation scale, value scale
COSTUMES = [
    # art name,        base art,    head(accessory), female, level, stats,            cost,  hue, sat, val
    ("santazombie", "zombieman", "generic", False, 5,
     {"Energy": 120, "Speed": 5, "AttackStrength": 6, "CookSpeedBonus": 2}, 25000,
     0, 1.35, 1.05),
    ("santaclauszombie", "cutegirl", "genericfemale", True, 5,
     {"Energy": 115, "Speed": 6, "AttackStrength": 5, "TipMultiplier": 2}, 25000,
     0, 1.35, 1.05),
    ("phantomzombie", "zombieman", "oldman", False, 10,
     {"Energy": 150, "Speed": 6, "AttackStrength": 8, "TipRating": 4}, 60000,
     260, 1.4, 0.85),
    ("neonzombie", "cutegirl", "genericfemale", True, 8,
     {"Energy": 130, "Speed": 7, "AttackStrength": 6, "TipMultiplier": 2}, 45000,
     140, 1.8, 1.15),
    ("frostzombie", "businesswoman", "businesswoman", True, 12,
     {"Energy": 160, "Speed": 5, "AttackStrength": 9, "CookXPBonus": 2}, 80000,
     190, 1.3, 1.1),
]


def find_base_folder(base):
    for suffix in ("-zombie", "-human"):
        for sheet in ("characterParts2", "characterParts3", "characterParts4",
                      "characterParts"):
            p = os.path.join(IMAGES, sheet, base + suffix)
            if os.path.isdir(p):
                return p
    return None


def recolor(src, dst, hue_deg, sat_mul, val_mul):
    im = Image.open(src).convert("RGBA")
    px = im.load()
    w, h = im.size
    import colorsys
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            hh, ss, vv = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
            hh = (hh + hue_deg / 360.0) % 1.0
            ss = min(1.0, ss * sat_mul)
            vv = min(1.0, vv * val_mul)
            rr, gg, bb = colorsys.hsv_to_rgb(hh, ss, vv)
            px[x, y] = (int(rr * 255), int(gg * 255), int(bb * 255), a)
    im.save(dst)


def build_sheet(base, art, hue, sat, val):
    src = find_base_folder(base)
    if not src:
        print(f"  !! base folder for '{base}' not found - skipping")
        return False
    made = False
    for suffix in ("-zombie", "-human"):
        src_dir = find_base_folder(base)  # prefers -zombie
        if suffix == "-human":
            human = os.path.join(os.path.dirname(src_dir), base + "-human")
            if os.path.isdir(human):
                src_dir = human
        dst_dir = os.path.join(OUT_SHEET, art + suffix)
        os.makedirs(dst_dir, exist_ok=True)
        for name in os.listdir(src_dir):
            s = os.path.join(src_dir, name)
            d = os.path.join(dst_dir, name)
            if name.lower().endswith(".png.json") or name == "0-spacer.png":
                shutil.copyfile(s, d)
            elif name.lower().endswith(".png"):
                recolor(s, d, hue, sat, val)
            else:
                shutil.copyfile(s, d)
        made = True
        print(f"  sheet: {os.path.relpath(dst_dir, ROOT)}")
    return made


def main():
    with open(CHARS_JSON, encoding="utf-8") as f:
        rows = json.load(f)

    base_by_id = {}
    for r in rows:
        zid = r.get("ZcrId", "")
        if not zid.startswith("char.costume."):
            base_by_id.setdefault(r.get("CharacterArtString", ""), r)
            base_by_id["id:" + zid] = r

    # drop previously generated costume rows (idempotent re-runs)
    rows = [r for r in rows if not r.get("ZcrId", "").startswith("char.costume.")]

    for art, base, head, female, level, stats, cost, hue, sat, val in COSTUMES:
        print(f"costume '{art}' (base {base})")
        base_row = base_by_id.get("id:char." + base) or base_by_id.get(base)
        if not base_row:
            # fallback: first row with the right gender
            for r in rows:
                if r.get("IsFemale", False) == female and \
                        not r.get("ZcrId", "").startswith("char.costume."):
                    base_row = r
                    break
        if not base_row:
            print("  !! no base row found - skipping")
            continue
        if not build_sheet(base, art, hue, sat, val):
            continue
        row = dict(base_row)
        row["ZcrId"] = "char.costume." + art
        row["Name"] = art.replace("zombie", " Zombie").title().replace("  ", " ")
        row["CharacterArtString"] = art
        row["CharacterArtStringHead"] = head
        row["IsFemale"] = female
        row["CafeLevelRequired"] = level
        for k, v in stats.items():
            row[k] = v
        row["Cost"] = cost
        row["PurchaseWithToxin"] = False
        rows.append(row)
        print(f"  row: {row['ZcrId']} ({row['Name']}) level {level}")

    with open(CHARS_JSON, "w", encoding="utf-8") as f:
        json.dump(rows, f, indent=4)
    print(f"wrote {CHARS_JSON} ({len(rows)} rows)")


if __name__ == "__main__":
    sys.exit(main())
