#!/usr/bin/env python3
"""Compare the apworld's location rules with OoT3D_Randomizer's own logic.

Both sides get the same item sets, nothing placed in the world, and answer
which locations can be reached. A location the apworld reaches but the 3D
randomizer does not is a rule that is too permissive for the 3DS game (a
seed could expect the impossible); the other direction is a rule that is
stricter than needed. Each difference is reported with the smallest item set
seen that shows it.

The 3D side is logic_oracle/oot3d_logic_oracle.exe, built from an
OoT3D_Randomizer checkout by logic_oracle/build_oracle.ps1, and set to the
game's vanilla behaviour that the apworld's supported options describe.

Run with Archipelago's Python, from anywhere:
  compare_logic.py --archipelago <Archipelago checkout with worlds/oot3d>
                   --oracle <oot3d_logic_oracle.exe> [--samples N] [--report out.json]
"""

from __future__ import annotations

import argparse
import collections
import itertools
import json
import os
import random
import subprocess
import sys
import tempfile
from pathlib import Path

# 3D randomizer options for the behaviour the apworld supports (oot3d_options.py):
# the unmodified game, dungeon items in their own dungeon, glitchless logic.
ORACLE_SETTINGS = {
    "Forest": "Closed",
    "Kakariko Gate": "Closed",
    # "Closed" opens the door with the Song of Time alone (a randomizer
    # patch); "Intended" also needs the spiritual stones, like the game.
    "Door of Time": "Intended",
    "Zora's Fountain": "Normal",
    "Jabu-Jabu": "Closed",
    "Gerudo Fortress": "Normal",
    "Rainbow Bridge": "Vanilla",
    "Random Ganon's Trials": "Off",
    "Trial Count": "6",
    "Starting Age": "Child",
    "Ammo Drops": "On",
    "Heart Drops and Refills": "On",
    "Bombchus in Logic": "Off",
    "MQ Dungeon Count": "0",
    "Enemy Randomizer": "Off",
    "Small Keys": "Own Dungeon",
    "Boss Keys": "Own Dungeon",
    "Maps/Compasses": "Own Dungeon",
    "Ganon's Boss Key": "Vanilla",
    "Gerudo Fortress Keys": "Vanilla",
    "Key Rings": "All Off",
    "Skip Child Stealth": "Don't Skip",
    "Skip Tower Escape": "Don't Skip",
    "Skip Epona Race": "Don't Skip",
    "Minigames repetitions": "Don't Skip",
    "Free Scarecrow": "Off",
    "Four Poes Cutscene": "Don't Skip",
    "Lake Hylia Owl": "Don't Skip",
    "Big Poe Target Count": "10",
    "Cuccos to return": "7",
    "King Zora Speed": "Vanilla",
    "Complete Mask Quest": "Off",
    "Logic": "Glitchless",
    "Night GSs Expect Sun's": "Off",
    "Starting Time": "Day",
    "Lift Age Restrictions": "All Disabled",
    "Damage Multiplier": "x1",
    "Gloom Mode": "Off",
    "Random GS Locations": "Off",
    "Random Ocarina Melodies": "Off",
}

# Items whose effect the 3D logic reads, under names the apworld does not
# use, or the other way round. Filled from the run's "unknown item" report.
ITEM_ALIASES: dict[str, str] = {}

PROGRESSION_STEPS = ("generate_early", "create_regions", "create_items", "set_rules",
                     "connect_entrances", "generate_basic", "pre_fill")


class OracleSession:
    """One oracle process answering queries one line at a time."""

    def __init__(self, oracle: Path, checks_file: Path) -> None:
        args = [str(oracle), "--vanilla-except", str(checks_file)]
        for name, value in ORACLE_SETTINGS.items():
            args += ["--set", f"{name}={value}"]
        self._process = subprocess.Popen(args + ["--query"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                         stderr=subprocess.DEVNULL, text=True, encoding="utf-8")

    def query(self, items: list[str]) -> set[str] | str:
        self._process.stdin.write("|".join(ITEM_ALIASES.get(name, name) for name in items) + "\n")
        self._process.stdin.flush()
        line = self._process.stdout.readline()
        if not line:
            raise RuntimeError("the oracle stopped answering")
        line = line.rstrip("\n")
        return line if line.startswith("error:") else set(filter(None, line.split("|")))

    def close(self) -> None:
        self._process.stdin.close()
        self._process.wait()


def minimize(items: list[str], shows_difference) -> list[str]:
    """Drop items one at a time while the difference still shows."""
    kept = list(items)
    index = 0
    while index < len(kept):
        candidate = kept[:index] + kept[index + 1:]
        if shows_difference(candidate):
            kept = candidate
        else:
            index += 1
    return kept


def oracle_items(oracle: Path) -> dict[str, bool]:
    args = [str(oracle)]
    for name, value in ORACLE_SETTINGS.items():
        args += ["--set", f"{name}={value}"]
    out = subprocess.run(args + ["--list-items"], capture_output=True, text=True, encoding="utf-8",
                         check=True).stdout
    return {name: flag == "1" for name, flag in (line.split("\t") for line in out.splitlines() if "\t" in line)}


def oracle_locations(oracle: Path) -> set[str]:
    args = [str(oracle)]
    for name, value in ORACLE_SETTINGS.items():
        args += ["--set", f"{name}={value}"]
    out = subprocess.run(args + ["--list-locations"], capture_output=True, text=True, encoding="utf-8",
                         check=True).stdout
    return set(out.splitlines())


class ApworldLogic:
    def __init__(self, archipelago: Path, world_seed: int = 1) -> None:
        os.environ.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
        os.chdir(archipelago)
        sys.path.insert(0, str(archipelago))
        sys.argv = [sys.argv[0]]
        import logging
        logging.disable(logging.CRITICAL)
        import ModuleUpdate  # noqa: F401  (sets up the import paths like Generate.py)
        from BaseClasses import CollectionState
        from test.general import setup_solo_multiworld
        from worlds.AutoWorld import AutoWorldRegister

        self._CollectionState = CollectionState
        world_type = AutoWorldRegister.world_types["Ocarina of Time 3D"]
        self.multiworld = setup_solo_multiworld(world_type, PROGRESSION_STEPS, seed=world_seed)
        self.player = 1
        self.world = self.multiworld.worlds[self.player]
        self.locations = [location for location in self.multiworld.get_locations(self.player)
                          if location.address is not None]
        self.events = [location for location in self.multiworld.get_locations(self.player)
                       if location.address is None and location.item is not None]

        # Checks the world filled with their own vanilla item (songs, dungeon
        # rewards, the egg) act like events: their item comes from there, on
        # both sides. Other pre-placed items (keys in their own dungeon, at a
        # random spot) count as free items, like the item pool.
        data = Path(sys.modules[world_type.__module__].__file__).parent / "data"
        item_names = {item["enum"]: item["name"]
                      for item in json.loads((data / "items.json").read_text(encoding="utf-8"))["items"]}
        vanilla = {location["name"]: item_names.get(location["vanilla_item"])
                   for location in json.loads((data / "locations.json").read_text(encoding="utf-8"))["locations"]
                   if not location["master_quest"]}
        self.fixed = [location for location in self.locations
                      if location.item is not None and location.item.name == vanilla.get(location.name)]
        fixed = set(self.fixed)
        self.unfixed = [location for location in self.locations if location not in fixed]

    def progression_pool(self) -> list[str]:
        """The progression items of a generated pool, with their counts: the
        item pool and what the world placed at checks that are not their
        vanilla item. Precollected items are not part of it; every state
        starts with them."""
        pool = [item.name for item in self.multiworld.itempool
                if item.player == self.player and item.advancement]
        pool += [location.item.name for location in self.unfixed
                 if location.item is not None and location.item.advancement]
        return pool

    def reachable(self, items: list[str]) -> set[str]:
        state = self._CollectionState(self.multiworld)
        for name in items:
            state.collect(self.world.create_item(name), prevent_sweep=True)
        state.sweep_for_advancements(locations=self.events + self.fixed)
        return {location.name for location in self.locations if location.can_reach(state)}


def item_sets(pool: list[str], samples: int, rng: random.Random):
    """Named item sets: none; all; all with only k copies of one item (k from
    0, which tests key counts and progressive steps); all without two item
    kinds (which tests rules with alternatives); random parts of the pool."""
    counts = collections.Counter(pool)
    kinds = sorted(counts)
    yield "nothing", []
    yield "everything", list(pool)
    for name in kinds:
        others = [item for item in pool if item != name]
        for kept in range(counts[name]):
            yield (f"everything without {name}" if kept == 0 else f"everything with {kept} {name}",
                   others + [name] * kept)
    for first, second in itertools.combinations(kinds, 2):
        yield f"everything without {first} and {second}", [item for item in pool if item not in (first, second)]
    for index in range(samples):
        keep = rng.uniform(0.2, 0.8)
        yield f"random {index}", [item for item in pool if rng.random() < keep]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--archipelago", type=Path, required=True)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=200)
    parser.add_argument("--seed", type=int, default=1, help="seed for the random item sets")
    parser.add_argument("--world-seed", type=int, default=1,
                        help="generation seed of the apworld, which decides where pre-placed keys land")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--minimize", action="store_true",
                        help="shrink each difference's example to a smallest item set that still shows it")
    arguments = parser.parse_args()
    oracle = arguments.oracle.resolve()
    report_path = arguments.report.resolve() if arguments.report else None

    known_3d_items = oracle_items(oracle)
    known_3d_locations = oracle_locations(oracle)
    logic = ApworldLogic(arguments.archipelago.resolve(), arguments.world_seed)
    pool = logic.progression_pool()

    unknown = sorted({name for name in pool if ITEM_ALIASES.get(name, name) not in known_3d_items})
    if unknown:
        print("apworld progression items the 3D logic does not know:", ", ".join(unknown))
    pool = [name for name in pool if ITEM_ALIASES.get(name, name) in known_3d_items]
    compared = sorted(location.name for location in logic.locations if location.name in known_3d_locations)
    only_apworld = sorted(location.name for location in logic.locations if location.name not in known_3d_locations)
    if only_apworld:
        print(f"{len(only_apworld)} apworld locations are not in the 3D pool, e.g. {only_apworld[:5]}")
    compared_set = set(compared)

    print(f"{len(logic.fixed)} checks hold their vanilla item and act as events")
    empty_checks = sorted(location.name for location in logic.unfixed if location.name in known_3d_locations)

    sets = list(item_sets(pool, arguments.samples, random.Random(arguments.seed)))
    with tempfile.TemporaryDirectory() as scratch:
        checks_file = Path(scratch) / "checks.txt"
        checks_file.write_text("".join(name + "\n" for name in empty_checks), encoding="utf-8")
        session = OracleSession(oracle, checks_file)
        try:
            differences = compare(logic, session, sets, compared_set)
            if arguments.minimize:
                for name, directions in differences.items():
                    for direction, entry in directions.items():
                        entry.append(minimize(entry[2], lambda items: shows(logic, session, items, name, direction)))
        finally:
            session.close()

    report = {"compared": len(compared), "item_sets": len(sets),
              "only_apworld_locations": only_apworld, "unknown_items": unknown, "differences": {}}
    print(f"compared {len(compared)} locations over {len(sets)} item sets")
    for direction, title in (("apworld_only", "apworld more permissive than 3D"),
                             ("oracle_only", "apworld stricter than 3D")):
        names = sorted(name for name, entry in differences.items() if direction in entry)
        print(f"{title}: {len(names)} locations")
        for name in names:
            entry = differences[name][direction]
            minimal = f"; minimal: {' + '.join(entry[3]) or 'nothing'}" if len(entry) > 3 else ""
            print(f"  {'+' if direction == 'apworld_only' else '-'} {name}  ({entry[0]} sets, e.g. {entry[1]}{minimal})")
            report["differences"].setdefault(name, {})[direction] = {
                "count": entry[0], "example": entry[1], "items": sorted(collections.Counter(entry[2]).items()),
                **({"minimal": entry[3]} if len(entry) > 3 else {})}
    if report_path:
        report_path.write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
    return 0


def shows(logic: ApworldLogic, session: OracleSession, items: list[str], name: str, direction: str) -> bool:
    oracle_answer = session.query(items)
    if isinstance(oracle_answer, str):
        return False
    apworld = name in logic.reachable(items)
    return (apworld and name not in oracle_answer) if direction == "apworld_only" else \
        (name in oracle_answer and not apworld)


def compare(logic: ApworldLogic, session: OracleSession, sets, compared: set[str]):
    """location -> direction -> [count, label of the smallest set, its items]"""
    differences: dict[str, dict[str, list]] = collections.defaultdict(dict)
    for label, items in sets:
        oracle_answer = session.query(items)
        if isinstance(oracle_answer, str):
            print(f"{label}: {oracle_answer}")
            continue
        apworld = logic.reachable(items) & compared
        oracle_reached = oracle_answer & compared
        for direction, names in (("apworld_only", apworld - oracle_reached),
                                 ("oracle_only", oracle_reached - apworld)):
            for name in names:
                entry = differences[name].setdefault(direction, [0, label, items])
                entry[0] += 1
                if len(items) < len(entry[2]):
                    entry[1], entry[2] = label, items
    return differences

if __name__ == "__main__":
    sys.exit(main())
