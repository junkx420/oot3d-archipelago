#!/usr/bin/env python3
"""Verify the SoH -> 3D location name map against both games' location keys.

Ship of Harkinian's randomizer lists every check with its scene and flag
(soh/soh/Enhancements/randomizer/location_list.cpp); OoT3D_Randomizer's keys
are in the generated data/locations.json. The SoH apworld's Locations enum
identifiers are SoH's RandomizerCheck names without "RC_". For chests and
collectables the pair (scene, flag) must agree; for Gold Skulltulas the GS
index and flag. Base and delayed locations can only be checked by scene.

Also prints, for 3D locations without a mapped SoH name, every SoH check of
the same kind with the same (scene, flag), to fill the overrides from data.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ENTRY_RE = re.compile(r"locationTable\[RC_(\w+)\]\s*=\s*Location::(\w+)\((.*?)\);", re.S)
SCENE_RE = re.compile(r"/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*DEFINE_SCENE\(\w+,\s*\w+,\s*(SCENE_\w+)")


def split_arguments(text: str) -> list[str]:
    parts, depth, quote, current = [], 0, False, []
    for char in text:
        if quote:
            current.append(char)
            if char == '"':
                quote = False
        elif char == '"':
            quote = True
            current.append(char)
        elif char in "({[":
            depth += 1
            current.append(char)
        elif char in ")}]":
            depth -= 1
            current.append(char)
        elif char == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    if "".join(current).strip():
        parts.append("".join(current).strip())
    return parts


def soh_checks(location_list: Path, scene_table: Path) -> dict[str, dict]:
    scenes = {name: int(value, 16) for value, name in SCENE_RE.findall(scene_table.read_text(encoding="utf-8"))}
    checks = {}
    for rc, kind, arguments in ENTRY_RE.findall(location_list.read_text(encoding="utf-8")):
        parts = split_arguments(arguments)
        scene_index = next((i for i, p in enumerate(parts) if p.startswith("SCENE_")), None)
        if scene_index is None:
            continue
        check = {"kind": kind, "scene": scenes.get(parts[scene_index])}
        numbers = []
        for part in parts[scene_index + 1:]:
            if part.startswith('"'):
                break
            try:
                numbers.append(int(part, 0))
            except ValueError:
                numbers.append(None)
        if kind == "GSToken":
            check["flag"] = numbers[1] if len(numbers) > 1 else None
            # Overworld tokens name their GS index last; dungeon tokens use
            # the dungeon's scene as the index.
            try:
                check["gs_scene"] = int(parts[-1], 0)
            except ValueError:
                check["gs_scene"] = check["scene"]
        elif kind in ("Chest", "Collectable"):
            check["flag"] = numbers[1] if len(numbers) > 1 else None
        checks[rc] = check
    return checks


def find_check(checks: dict[str, dict], identifier: str) -> dict | None:
    """The apworld's identifiers differ from SoH's RC names in spelling only
    (ITEM1 / ITEM_1, MIDO / MIDOS, HEART_CONTAINER / HEART)."""
    normalized = {rc.replace("_", ""): check for rc, check in checks.items()}
    for candidate in (identifier, identifier.replace("MIDO_", "MIDOS_"),
                      identifier.replace("HEART_CONTAINER", "HEART")):
        if candidate in checks:
            return checks[candidate]
        if candidate.replace("_", "") in normalized:
            return normalized[candidate.replace("_", "")]
    return None


def enum_values(path: Path, name: str) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    start = text.index(f"class {name}(")
    end = text.find("\nclass ", start + 1)
    return dict(re.findall(r'^\s+(\w+)\s*=\s*"([^"]*)"', text[start:end], re.M))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--shipwright", type=Path, required=True)
    parser.add_argument("--soh-enums", type=Path, required=True)
    parser.add_argument("--apworld", type=Path, required=True)
    arguments = parser.parse_args()

    checks = soh_checks(arguments.shipwright / "soh/soh/Enhancements/randomizer/location_list.cpp",
                        arguments.shipwright / "soh/include/tables/scene_table.h")
    soh_identifier = {value: key for key, value in enum_values(arguments.soh_enums, "Locations").items()}
    data = arguments.apworld / "data"
    name_map = json.loads((data / "soh_name_map.json").read_text(encoding="utf-8"))["locations"]
    locations = {l["name"]: l for l in json.loads((data / "locations.json").read_text(encoding="utf-8"))["locations"]}

    verified = scene_only = 0
    problems = []
    for soh_name, name_3d in name_map.items():
        location = locations[name_3d]
        check = find_check(checks, soh_identifier[soh_name])
        if check is None:
            problems.append(f"{soh_name}: no SoH location_list entry")
            continue
        kind = location["kind"]
        if kind in ("Chest", "Collectable") and check["kind"] == kind:
            ok = check["scene"] == location["scene"] and check["flag"] == location["flag"]
        elif kind == "GSToken" and check["kind"] == "GSToken":
            ok = check["gs_scene"] == location["scene"] and check["flag"] == location["flag"]
        else:
            scene_only += 1
            continue
        if ok:
            verified += 1
        else:
            problems.append(f"{soh_name} -> {name_3d}: SoH {check} vs 3D scene 0x{location['scene']:02X} "
                            f"flag 0x{location['flag']:02X} {kind}")
    print(f"verified by (scene, flag): {verified}; scene-only kinds not checked: {scene_only}")
    for problem in problems:
        print("MISMATCH", problem)

    mapped_3d = set(name_map.values())
    for name_3d, location in locations.items():
        if name_3d in mapped_3d or location["master_quest"] or location["kind"] not in ("Chest", "Collectable", "GSToken"):
            continue
        same = [rc for rc, check in checks.items() if check["kind"] == location["kind"]
                and check.get("flag") == location["flag"]
                and (check.get("gs_scene") if location["kind"] == "GSToken" else check["scene"]) == location["scene"]]
        if same:
            print(f"CANDIDATE {name_3d} -> {', '.join(same)}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
