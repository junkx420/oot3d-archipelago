#!/usr/bin/env python3
"""Check the apworld's glitched logic (oot3d_glitched.py) against OoT3D_Randomizer's.

Each run sets logic_rules to glitched in the apworld and Logic to Glitched in
the oracle, with the same glitch settings, and compares over
compare_logic.py's item sets. A run passes when no location is reachable in
the apworld but not in the 3D logic, beyond what the glitchless baseline
already shows (the apworld may be stricter: it leaves out conditions it
cannot translate). Runs: every glitch at its highest level, then each
category and each miscellaneous glitch alone.

Run with Archipelago's Python:
  verify_glitches.py --archipelago <checkout> --oracle <exe> --report glitches.json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import compare_logic as cl  # noqa: E402
from verify_tricks import check  # noqa: E402

# apworld option -> (oracle setting name, highest level).
CATEGORIES = {
    "glitch_restricted_items": ("Restricted Items", "Novice"),
    "glitch_super_stab": ("Super Stab", "Novice"),
    "glitch_isg": ("Infinite Sword Glitch", "Advanced"),
    "glitch_hover": ("Bomb Hover", "Advanced"),
    "glitch_bomb_oi": ("Ocarina Items (Bomb)", "Expert"),
    "glitch_hover_boost": ("HoverBoost", "Advanced"),
    "glitch_super_slide": ("Extended Super Slide", "Expert"),
    "glitch_megaflip": ("Megaflip", "Hero"),
    "glitch_a_slide": ("A-Slide", "Expert"),
    "glitch_hammer_slide": ("Hammer Slide", "Intermediate"),
    "glitch_ledge_cancel": ("Ledge Cancel", "Advanced"),
    "glitch_action_swap": ("Action Swap", "Advanced"),
    "glitch_qpa": ("Quick Put Away", "Expert"),
    "glitch_hookshot_clip": ("Hookshot Clip", "Intermediate"),
    "glitch_hookshot_jump_bonk": ("Hookshot Jump (Bonk)", "Advanced"),
    "glitch_hookshot_jump_boots": ("Hookshot Jump (Boots)", "Advanced"),
    "glitch_cutscene_dive": ("Cutscene Dives", "Advanced"),
    "glitch_navi_dive_stick": ("Navi Dive (Stick)", "Advanced"),
    "glitch_triple_slash_clip": ("Triple Slash Clip", "Expert"),
    "glitch_ledge_clip": ("Ledge Clip", "Advanced"),
    "glitch_seam_walk": ("Seam Walk", "Hero"),
}
MISC = {
    "glitch_wwt_escape": "WWT Kokiri Forest Escape",
    "glitch_gv_tent_as_child": "Enter GV Tent as Child",
    "glitch_gf_guard_sneak": "Sneak Past the GF Guard",
    "glitch_itemless_wasteland": "Cross the HW w/o Items",
    "glitch_occams_statue": "Occam's Statue",
    "glitch_zd_oob_jump_slash": "ZD OoB w/ Jump Slash",
    "glitch_jabu_stick_recoil": "Enter Jabu w/o Bottle",
    "glitch_jabu_adult": "Enter Jabu as Adult",
    "glitch_blue_fire_wall": "Break Walls w/ Blue Fire",
    "glitch_classic_halfie": "Classic Halfie",
    "glitch_modern_halfie": "Modern Halfie",
    "glitch_jabu_switch": "Jabu Switch w/ CS item",
    "glitch_forest_bk_skip": "Forest Temple BK Skip",
    "glitch_fire_grunz_clip": "Fire Temple Grunz Clip",
}


def settings(categories: list[str], misc: list[str]) -> tuple[dict, dict]:
    options = {"logic_rules": "glitched"}
    extra = {"Logic": "Glitched"}
    for name in categories:
        oracle_name, level = CATEGORIES[name]
        options[name] = level.lower()
        extra[oracle_name] = level
    for name in misc:
        options[name] = True
        extra[MISC[name]] = "On"
    return options, extra


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--archipelago", type=Path, required=True)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=60)
    parser.add_argument("--only", action="append", default=[], help="run only these runs (all, or an option name)")
    arguments = parser.parse_args()
    oracle = arguments.oracle.resolve()
    archipelago = arguments.archipelago.resolve()
    report_path = arguments.report.resolve()
    known_items = cl.oracle_items(oracle)
    known_locations = cl.oracle_locations(oracle)

    more, stricter, count = check(archipelago, oracle, {}, {}, arguments.samples, known_items, known_locations)
    report = {"baseline": {"more_permissive": more, "stricter": len(stricter), "item_sets": count}, "runs": []}
    print(f"baseline: {len(more)} more permissive, {len(stricter)} stricter", flush=True)
    runs = [("all", list(CATEGORIES), list(MISC))]
    runs += [(name, [name], []) for name in CATEGORIES] + [(name, [], [name]) for name in MISC]
    if arguments.only:
        runs = [run for run in runs if run[0] in arguments.only]
    for name, categories, misc in runs:
        options, extra = settings(categories, misc)
        more, stricter, _ = check(archipelago, oracle, options, extra, arguments.samples, known_items,
                                  known_locations)
        new = {location: example for location, example in more.items()
               if location not in report["baseline"]["more_permissive"]}
        report["runs"].append({"run": name, "verified": not new, "more_permissive": new, "stricter": len(stricter)})
        print(f"{'ok ' if not new else 'NO '} {name}: {len(stricter)} stricter"
              f"{'' if not new else '; more permissive: ' + ', '.join(sorted(new))}", flush=True)
        report_path.write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
    print(f"{sum(run['verified'] for run in report['runs'])} of {len(report['runs'])} runs verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
