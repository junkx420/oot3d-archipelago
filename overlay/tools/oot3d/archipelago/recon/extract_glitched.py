#!/usr/bin/env python3
"""Extract OoT3D_Randomizer's glitched logic from its source.

Every area (areaTable[AREA] = Area(...)) lists events, locations and exits,
each with a glitchless rule and, for some, a second /*Glitched*/ rule. Gold
Skulltula tables (gsTable[...]) do the same per token. This writes every
entry that has a glitched rule, with both rule bodies as C++ expressions:

  extract_glitched.py --randomizer <OoT3D_Randomizer checkout> --out glitched.json
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ENTRY = re.compile(r"\b(Entrance|LocationAccess|EventAccess)\s*\(")
AREA = re.compile(r"areaTable\[([A-Z0-9_]+)\]\s*=\s*Area\(")
GS = re.compile(r"gsTable\[([A-Z0-9_]+)\]\s*=\s*new\s+GoldSkulltulaData\(")
LAMBDA = re.compile(r"\[\]\s*\{")


def strip_comments(text: str) -> str:
    text = re.sub(r"//[^\n]*", "", text)
    return re.sub(r"/\*(?!\s*Glitched\s*\*/).*?\*/", "", text, flags=re.S)


def balanced(text: str, start: int, open_char: str, close_char: str) -> int:
    """Index after the bracket matching text[start] (an opening one)."""
    depth = 0
    for index in range(start, len(text)):
        if text[index] == open_char:
            depth += 1
        elif text[index] == close_char:
            depth -= 1
            if depth == 0:
                return index + 1
    raise ValueError("unbalanced")


def lambdas(arguments: str) -> list[tuple[bool, str]]:
    """The lambda bodies in an argument list, each with whether a /*Glitched*/
    marker precedes it."""
    found = []
    for match in LAMBDA.finditer(arguments):
        open_brace = match.end() - 1
        end = balanced(arguments, open_brace, "{", "}")
        body = arguments[open_brace + 1:end - 1].strip()
        glitched = re.search(r"/\*\s*Glitched\s*\*/\s*$", arguments[:match.start()]) is not None
        found.append((glitched, body))
    return found


def first_name(arguments: str) -> str:
    match = re.match(r"\s*&?\s*([A-Za-z0-9_:]+)", arguments)
    return match.group(1) if match else ""


def extract(path: Path) -> list[dict]:
    text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    areas = [(match.start(), match.group(1)) for match in AREA.finditer(text)]
    entries = []
    for match in ENTRY.finditer(text):
        open_paren = match.end() - 1
        end = balanced(text, open_paren, "(", ")")
        arguments = text[open_paren + 1:end - 1]
        rules = lambdas(arguments)
        glitched = [body for is_glitched, body in rules if is_glitched]
        plain = [body for is_glitched, body in rules if not is_glitched]
        area = next((name for start, name in reversed(areas) if start < match.start()), "")
        entries.append({"file": path.name, "area": area, "kind": match.group(1),
                        "target": first_name(arguments), "glitchless": plain[0] if plain else "",
                        "glitched": glitched[0] if glitched else ""})
    for match in GS.finditer(text):
        open_paren = match.end() - 1
        end = balanced(text, open_paren, "(", ")")
        arguments = text[open_paren + 1:end - 1]
        # Each placement: {AREA, GsScene{..}, Room{..}, { glitchless, /*Glitched*/ glitched } }
        for placement in re.finditer(r"\{\s*([A-Z0-9_]+)\s*,\s*GsScene", arguments):
            block_start = placement.start()
            block_end = balanced(arguments, block_start, "{", "}")
            rules = lambdas(arguments[block_start:block_end])
            glitched = [body for is_glitched, body in rules if is_glitched]
            plain = [body for is_glitched, body in rules if not is_glitched]
            entries.append({"file": path.name, "area": placement.group(1), "kind": "LocationAccess",
                            "target": match.group(1), "glitchless": plain[0] if plain else "",
                            "glitched": glitched[0] if glitched else ""})
            break  # the first placement is the vanilla one
    return entries


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--randomizer", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--data", type=Path, help="also write the apworld's data file (glitched rules only)")
    arguments = parser.parse_args()
    source = arguments.randomizer / "source"
    files = ([source / "location_access.cpp"] + sorted((source / "location_access").glob("*.cpp"))
             + sorted((source / "gold_skulltulas").glob("*.cpp")))
    entries = [entry for path in files for entry in extract(path)]
    glitched = [entry for entry in entries if entry["glitched"]]
    areas = sorted({entry["area"] for entry in entries if entry["area"]})
    arguments.out.write_text(json.dumps({"areas": areas, "entries": glitched, "all_entries": len(entries)}, indent=1)
                             + "\n", encoding="utf-8")
    if arguments.data:
        rules = [{key: entry[key] for key in ("area", "kind", "target")}
                 | {"rule": " ".join(entry["glitched"].split())} for entry in glitched]
        # Glitchless conditions whose key count rises with glitched logic
        # (SmallKeys(dungeon, glitchless, glitched): some glitches can waste keys).
        keys = []
        for entry in entries:
            for dungeon, plain, more in re.findall(r"SmallKeys\(\s*([A-Z_]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)",
                                                   entry["glitchless"]):
                if int(more) > int(plain):
                    keys.append({key: entry[key] for key in ("area", "kind", "target")}
                                | {"dungeon": dungeon, "count": int(more)})
        arguments.data.write_text(json.dumps({
            "source": "OoT3D_Randomizer source/location_access and source/gold_skulltulas: the /*Glitched*/ "
                      "conditions, written by recon/extract_glitched.py",
            "rules": rules, "keys": keys}, indent=0) + "\n", encoding="utf-8")
    print(f"{len(entries)} entries in {len(areas)} areas, {len(glitched)} with a glitched rule")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
