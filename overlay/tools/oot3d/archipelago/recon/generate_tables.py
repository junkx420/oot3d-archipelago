#!/usr/bin/env python3
"""Generate the Archipelago location and item tables from OoT3D_Randomizer.

Inputs (a checkout of gamestabled/OoT3D_Randomizer):
  source/item_location.cpp   locationTable[...] = ItemLocation::Kind(...)
  source/item_list.cpp       itemTable[...] = Item(...)
  shared/s_item.h            GetItemID values (/* 0xNN */ GI_...)

Outputs:
  --client   runtime/triaevum_resources/archipelago/locations.json, read by
             the game (format triaevum_archipelago_locations_v1)
  --apworld  the apworld's data/locations.json and data/items.json

Ids are base + index and append-only (decided 2026-10-25): an existing
output keeps every id it already assigned, keyed by the randomizer's enum
name; new entries are appended. Removing an entry from the randomizer keeps
its id reserved. The tables hold facts (names, scenes, flags, item ids),
not game code; the source commit is recorded in every output.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

LOCATION_BASE = 0x3D0000  # 3,997,696
ITEM_BASE = 0x3D0000

# ItemLocationType order equals the in-game ItemOverride type (item_location.hpp).
LOCATION_TYPES = {
    "Base": 0, "Chest": 1, "Collectable": 2, "GSToken": 3,
    "GrottoScrub": 4, "Delayed": 5, "TempleReward": 6, "Reward": 6,
}
# Hint locations are not item locations.
SKIPPED_KINDS = {"HintStone", "OtherHint", "OnInit"}
# Ganon's "location" holds the Triforce: the goal, not an item location.
SKIPPED_LOCATIONS = {"GANON", "NONE"}
ENUM_HEADERS = ("shared/s_dungeon.h", "shared/s_item_override.h")
ENUM_RE = re.compile(r"typedef\s+enum\s*\w*\s*\{(.*?)\}\s*\w+\s*;", re.S)

ITEM_CLASSES = {"ITEMCLASS_ADVANCEMENT": "progression", "ITEMCLASS_JUNK": "filler",
                "ITEMCLASS_NONE": "useful"}

LOCATION_RE = re.compile(r"locationTable\[(\w+)\]\s*=\s*ItemLocation::(\w+)\s*\((.*?)\);", re.S)
ITEM_RE = re.compile(r"itemTable\[(\w+)\]\s*=\s*Item\((.*?)\);", re.S)
GI_RE = re.compile(r"/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*(GI_\w+)")
STRING_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def source_text(path: Path) -> str:
    """The file without block comments and whole-line // comments, which
    hold disabled table rows."""
    text = re.sub(r"/\*.*?\*/", "", path.read_text(encoding="utf-8"), flags=re.S)
    return re.sub(r"^\s*//[^\n]*", "", text, flags=re.M)


def split_arguments(text: str) -> list[str]:
    """Split at top-level commas, keeping braces, parentheses and strings."""
    parts, depth, quote, current = [], 0, False, []
    previous = ""
    for char in text:
        if quote:
            current.append(char)
            if char == '"' and previous != "\\":
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
        previous = char
    if "".join(current).strip():
        parts.append("".join(current).strip())
    return parts


def parse_enum_constants(root: Path) -> dict[str, int]:
    """Values of the C enums that location flags use (DUNGEON_*, DLYOVR_*)."""
    constants: dict[str, int] = {}
    for header in ENUM_HEADERS:
        text = re.sub(r"//[^\n]*|/\*.*?\*/", "", (root / header).read_text(encoding="utf-8"), flags=re.S)
        for body in ENUM_RE.findall(text):
            value = -1
            for member in body.split(","):
                member = member.strip()
                if not member:
                    continue
                name, _, explicit = member.partition("=")
                value = int(explicit.strip(), 0) if explicit.strip() else value + 1
                constants[name.strip()] = value
    return constants


def number(text: str, constants: dict[str, int]) -> int:
    text = text.strip()
    if text in constants:
        return constants[text]
    return int(text, 0)


def parse_locations(path: Path, constants: dict[str, int]) -> list[dict]:
    text = source_text(path)
    locations = []
    for enum, kind, arguments in LOCATION_RE.findall(text):
        if kind in SKIPPED_KINDS or enum in SKIPPED_LOCATIONS:
            continue
        if kind not in LOCATION_TYPES:
            raise ValueError(f"unknown location kind {kind} for {enum}")
        parts = split_arguments(arguments)
        scene, flag = number(parts[0], constants), number(parts[1], constants)
        name = STRING_RE.fullmatch(parts[2]).group(1)
        vanilla = "GOLD_SKULLTULA_TOKEN" if kind == "GSToken" else parts[4]
        if not re.fullmatch(r"\w+", vanilla):
            raise ValueError(f"cannot read the vanilla item of {enum}: {parts}")
        locations.append({
            "enum": enum,
            "name": name,
            "kind": "TempleReward" if kind == "Reward" else kind,
            "scene": scene,
            "type": LOCATION_TYPES[kind],
            "flag": flag,
            "master_quest": "_MQ_" in enum or " MQ " in name,
            "vanilla_item": vanilla,
        })
    return locations


def parse_get_item_ids(path: Path) -> dict[str, int]:
    """GetItemID by counting the enum; later members carry no value comment.
    Every /* 0xNN */ comment present must agree with the count."""
    raw = path.read_text(encoding="utf-8")
    commented = {name: int(value, 16) for value, name in GI_RE.findall(raw)}
    body = re.search(r"typedef\s+enum\s+GetItemID\s*\{(.*?)\}\s*GetItemID\s*;", raw, re.S)
    if body is None:
        raise ValueError("GetItemID enum not found")
    text = re.sub(r"//[^\n]*|/\*.*?\*/", "", body.group(1), flags=re.S)
    values: dict[str, int] = {}
    value = -1
    for member in text.split(","):
        member = member.strip()
        if not member:
            continue
        name, _, explicit = member.partition("=")
        value = int(explicit.strip(), 0) if explicit.strip() else value + 1
        values[name.strip()] = value
    for name, expected in commented.items():
        if values.get(name) != expected:
            raise ValueError(f"GetItemID count disagrees with its comment for {name}")
    return values


def parse_string_constants(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    return dict(re.findall(r'const\s+char\s*\*\s*const\s+(\w+)\s*=\s*"((?:[^"\\]|\\.)*)"', text))


def parse_items(path: Path, get_item_ids: dict[str, int],
                string_constants: dict[str, str]) -> list[dict]:
    text = source_text(path)
    items = []
    for enum, arguments in ITEM_RE.findall(text):
        if enum == "NONE":
            continue
        parts = split_arguments(arguments)
        item_type, gi, item_class = parts[0], parts[1], parts[2]
        if gi.startswith("GI_"):
            if gi not in get_item_ids:
                raise ValueError(f"unknown get-item id {gi} for {enum}")
            gi_value = get_item_ids[gi]
        else:
            gi_value = int(gi, 0)
        # The English name is the first Text{} argument: a literal or a
        # shared_data.c string constant (dungeon key names).
        text_argument = next(p for p in parts if p.startswith("Text{"))
        first = split_arguments(text_argument[len("Text{"):-1])[0]
        literal = STRING_RE.fullmatch(first)
        if literal is not None:
            name = literal.group(1)
        elif first in string_constants:
            name = string_constants[first]
        else:
            raise ValueError(f"cannot resolve the English name of {enum}: {first}")
        items.append({
            "enum": enum,
            "name": name,
            "item_type": item_type,
            "get_item_id": gi_value,
            "classification": ITEM_CLASSES.get(item_class, "useful"),
        })
    return items


def assign_ids(entries: list[dict], previous: list[dict] | None, base: int) -> None:
    known = {row["enum"]: row["id"] for row in previous or []}
    next_id = max(known.values(), default=base - 1) + 1
    for entry in entries:
        if entry["enum"] in known:
            entry["id"] = known[entry["enum"]]
        else:
            entry["id"] = next_id
            next_id += 1


def load_previous(path: Path, key: str) -> list[dict] | None:
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8")).get(key)


def unique_names(entries: list[dict], label: str) -> None:
    seen: dict[str, str] = {}
    for entry in entries:
        if entry["name"] in seen:
            raise ValueError(f"duplicate {label} name {entry['name']!r}: "
                             f"{seen[entry['name']]} and {entry['enum']}")
        seen[entry["name"]] = entry["enum"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--randomizer", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--apworld-data", type=Path, required=True)
    arguments = parser.parse_args()

    root = arguments.randomizer
    locations = parse_locations(root / "source/item_location.cpp", parse_enum_constants(root))
    items = parse_items(root / "source/item_list.cpp", parse_get_item_ids(root / "shared/s_item.h"),
                        parse_string_constants(root / "shared/shared_data.c"))
    item_enums = {item["enum"] for item in items}
    for location in locations:
        if location["vanilla_item"] not in item_enums:
            raise ValueError(f"{location['enum']}: unknown vanilla item {location['vanilla_item']}")
    unique_names(locations, "location")
    unique_names(items, "item")

    locations_out = arguments.apworld_data / "locations.json"
    items_out = arguments.apworld_data / "items.json"
    assign_ids(locations, load_previous(locations_out, "locations"), LOCATION_BASE)
    assign_ids(items, load_previous(items_out, "items"), ITEM_BASE)

    source = {"repository": "https://github.com/gamestabled/OoT3D_Randomizer",
              "commit": arguments.commit,
              "license": "see THIRD_PARTY_NOTICES.md (OoT3D_Randomizer data)"}
    arguments.apworld_data.mkdir(parents=True, exist_ok=True)
    locations_out.write_text(json.dumps({"format": "triaevum_oot3d_apworld_locations_v1",
                                         "source": source, "locations": locations},
                                        indent=1, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    items_out.write_text(json.dumps({"format": "triaevum_oot3d_apworld_items_v1",
                                     "source": source, "items": items},
                                    indent=1, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    client = [{"id": l["id"], "name": l["name"], "scene": l["scene"], "type": l["type"],
               "flag": l["flag"], "master_quest": l["master_quest"]} for l in locations]
    arguments.client.parent.mkdir(parents=True, exist_ok=True)
    arguments.client.write_text(json.dumps({"format": "triaevum_archipelago_locations_v1",
                                            "source": source, "locations": client},
                                           indent=1, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    kinds: dict[str, int] = {}
    for location in locations:
        kinds[location["kind"]] = kinds.get(location["kind"], 0) + 1
    print(f"{len(locations)} locations {kinds}, {len(items)} items")
    return 0


if __name__ == "__main__":
    sys.exit(main())
