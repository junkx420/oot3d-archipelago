#!/usr/bin/env python3
"""Map the imported Ship of Harkinian apworld's names to OoT3D_Randomizer names.

The apworld keeps SoH's Python identifiers (Enums.Locations, Enums.Items)
and their rules; only the display strings change to the 3D names, so each
location and item gets the id of the generated 3D table.

Output: data/soh_name_map.json with
  locations: {SoH name: 3D name}   for every SoH location that exists in 3D
  items:     {SoH name: 3D name}   likewise
and a report of what stays unmapped on either side.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

# Systematic prefix differences, applied to 3D names to find the SoH name.
LOCATION_PREFIXES = (
    ("MK ", "Market "),
    ("GY ", "Graveyard "),
    ("ToT ", "Market ToT "),
    ("Gerudo Training Grounds ", "Gerudo Training Ground "),
)

# Individual names: 3D name -> SoH name. Filled from the unmapped report;
# each was checked against the SoH region that holds the location.
LOCATION_OVERRIDES: dict[str, str] = {
    "LW Trade Odd Poultice": "LW Trade Odd Potion",
    "Deku Theater Skull Mask": "LW Deku Theater Skull Mask",
    "Deku Theater Mask of Truth": "LW Deku Theater Mask of Truth",
    "GF Gerudo Token": "GF Freed All Carpenters",
    "MK Chest Game First Room Chest": "Market Chest Game First Room Item",
    "MK Chest Game Second Room Chest": "Market Chest Game Second Room Item",
    "MK Chest Game Third Room Chest": "Market Chest Game Third Room Item",
    "MK Chest Game Fourth Room Chest": "Market Chest Game Fourth Room Item",
    "MK Chest Game Fifth Room Chest": "Market Chest Game Fifth Room Item",
    "GY Composers Grave Chest": "Graveyard Royal Family's Tomb Chest",
    "Gerudo Training Grounds Water Room Chest":
        "Gerudo Training Ground Underwater Silver Rupee Chest",
    "Ganon's Tower Boss Key Chest": "Ganon's Castle Tower Boss Key Chest",
    # SoH region HF_NEAR_KAK_GROTTO holds HF_GS_STONE_BRIDGE_TREE_GROTTO.
    "HF GS Near Kak Grotto": "HF GS Stone Bridge Tree Grotto",
    "OGC GS": "HC OGC GS",
    "HC GS Tree": "HC GS in Tree",
    "Song from Composers Grave": "Song from Royal Family's Tomb",
    # Paired by (scene 0x0C, collectible flag) with SoH's location_list.cpp
    # (recon/verify_soh_map.py), not by cell names.
    "GF North F1 Carpenter": "GF 1 Torch Carpenter",
    "GF North F2 Carpenter": "GF Dead End Carpenter",
    "GF South F1 Carpenter": "GF Double Cell Carpenter",
    "GF South F2 Carpenter": "GF Steep Slope Carpenter",
}
ITEM_OVERRIDES: dict[str, str] = {
    "Shard of Agony": "Stone of Agony",
    "Fire Arrow": "Fire Arrows",
    "Ice Arrow": "Ice Arrows",
    "Light Arrow": "Light Arrows",
    "Gerudo Token": "Gerudo Membership Card",
    "Odd Poultice": "Odd Potion",
    "Progressive Strength Upgrade": "Strength Upgrade",
    "Forest Temple Big Key": "Forest Temple Boss Key",
    "Fire Temple Big Key": "Fire Temple Boss Key",
    "Water Temple Big Key": "Water Temple Boss Key",
    "Spirit Temple Big Key": "Spirit Temple Boss Key",
    "Shadow Temple Big Key": "Shadow Temple Boss Key",
    "Ganon's Castle Big Key": "Ganon's Castle Boss Key",
    "Training Grounds Small Key": "Training Ground Small Key",
    "Training Grounds Key Ring": "Training Ground Key Ring",
    "Chest Game Small Key": "Treasure Game Small Key",
    "Piece of Heart (Treasure Chest Minigame)": "Piece of Heart (WINNER)",
    "Green Rupee (Treasure Chest Minigame)": "Treasure Game Green Rupee",
    "Gohma Soul": "Gohma's Soul",
    "Barinade Soul": "Barinade's Soul",
    "Phantom Ganon Soul": "Phantom Ganon's Soul",
    "Volvagia Soul": "Volvagia's Soul",
    "Morpha Soul": "Morpha's Soul",
    "Bongo Bongo Soul": "Bongo Bongo's Soul",
    "Twinrova Soul": "Twinrova's Soul",
    "Ganon Soul": "Ganon's Soul",
}


def enum_values(enums_text: str, name: str) -> dict[str, str]:
    start = enums_text.index(f"class {name}(")
    end = enums_text.find("\nclass ", start + 1)
    return dict(re.findall(r'^\s+(\w+)\s*=\s*"([^"]*)"', enums_text[start:end], re.M))


def candidates(name: str):
    yield name
    for old, new in LOCATION_PREFIXES:
        if name.startswith(old):
            yield new + name[len(old):]


def rename_enum_values(enums_text: str, class_name: str, renames: dict[str, str]) -> tuple[str, int]:
    """Replace the string values inside one enum class; identifiers stay."""
    start = enums_text.index(f"class {class_name}(")
    end = enums_text.find("\nclass ", start + 1)
    block = enums_text[start:end]
    count = 0

    def replace(match: re.Match) -> str:
        nonlocal count
        old = match.group(2)
        if old in renames and renames[old] != old:
            count += 1
            return f'{match.group(1)}"{renames[old]}"'
        return match.group(0)

    block = re.sub(r'^(\s+\w+\s*=\s*)"([^"]*)"', replace, block, flags=re.M)
    return enums_text[:start] + block + enums_text[end:], count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--apworld", type=Path, required=True)
    parser.add_argument("--soh-enums", type=Path, required=True,
                        help="the unmodified SoH Enums.py (Archipelago-SoH worlds/oot_soh)")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--apply", action="store_true",
                        help="rewrite the apworld's Enums.py display strings to the 3D names")
    arguments = parser.parse_args()

    # The map always comes from the unmodified SoH names, so --apply can run
    # again after the apworld's Enums.py was already rewritten.
    soh_enums = arguments.soh_enums.read_text(encoding="utf-8")
    soh_locations = set(enum_values(soh_enums, "Locations").values())
    soh_items = set(enum_values(soh_enums, "Items").values())
    enums = (arguments.apworld / "Enums.py").read_text(encoding="utf-8")
    data = arguments.apworld / "data"
    locations = json.loads((data / "locations.json").read_text(encoding="utf-8"))["locations"]
    items = json.loads((data / "items.json").read_text(encoding="utf-8"))["items"]

    location_map: dict[str, str] = {}
    unmapped_3d_locations = []
    for location in locations:
        override = LOCATION_OVERRIDES.get(location["name"])
        options = [override] if override else list(candidates(location["name"]))
        match = next((soh for soh in options if soh in soh_locations), None)
        if match is None:
            unmapped_3d_locations.append(location)
        elif match in location_map:
            raise ValueError(f"two 3D locations map to {match!r}")
        else:
            location_map[match] = location["name"]

    item_map: dict[str, str] = {}
    unmapped_3d_items = []
    for item in items:
        soh = ITEM_OVERRIDES.get(item["name"], item["name"])
        if soh in soh_items:
            item_map[soh] = item["name"]
        else:
            unmapped_3d_items.append(item)

    out = data / "soh_name_map.json"
    out.write_text(json.dumps({"format": "triaevum_oot3d_soh_name_map_v1",
                               "locations": dict(sorted(location_map.items())),
                               "items": dict(sorted(item_map.items()))},
                              indent=1, ensure_ascii=False) + "\n",
                   encoding="utf-8", newline="\n")
    print(f"locations mapped {len(location_map)} of {len(locations)} 3D; "
          f"items mapped {len(item_map)} of {len(items)} 3D")
    if arguments.apply:
        # Values still carrying a SoH name are renamed; values renamed by an
        # earlier run are 3D names already and stay. A rename must never make
        # two members share a name.
        enums, renamed_locations = rename_enum_values(enums, "Locations", location_map)
        enums, renamed_items = rename_enum_values(enums, "Items", item_map)
        for class_name in ("Locations", "Items"):
            values = list(enum_values(enums, class_name).values())
            duplicates = {value for value in values if values.count(value) > 1}
            if duplicates:
                raise ValueError(f"{class_name}: duplicate names after renaming: {sorted(duplicates)}")
        (arguments.apworld / "Enums.py").write_bytes(enums.encode("utf-8"))
        print(f"renamed {renamed_locations} location and {renamed_items} item values in Enums.py")
    if arguments.report:
        print("3D locations without a SoH name (non-MQ):")
        for location in unmapped_3d_locations:
            if not location["master_quest"]:
                print(f"  {location['kind']:12} {location['name']}")
        print("3D items without a SoH name:")
        for item in unmapped_3d_items:
            print(f"  {item['item_type']:24} {item['name']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
