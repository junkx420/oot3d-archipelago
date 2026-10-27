#!/usr/bin/env python3
"""Pair OoT3D_Randomizer's logical tricks with Ship of Harkinian's by what they do.

A trick's effect is the set of locations it makes reachable: with the same
item sets, once without and once with the trick, on each side (the 3D side
through the logic oracle, the apworld through its rules). A 3D trick is
paired with the SoH trick whose effect overlaps its own most (Jaccard). The
pairing is a candidate; compare_logic.py with both tricks on then shows
whether the apworld's rules for it are never more permissive than 3D's.

Run with Archipelago's Python:
  map_tricks.py --archipelago <checkout> --oracle <oot3d_logic_oracle.exe> --report tricks.json
"""

from __future__ import annotations

import argparse
import collections
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import compare_logic as cl  # noqa: E402


def effect_sets(pool: list[str], samples: int) -> list[list[str]]:
    """Everything, everything without one kind of item (a trick that stands in
    for one item), and random parts of the pool (one that stands in for
    several)."""
    kinds = sorted(set(pool))
    sets = [list(pool)] + [[item for item in pool if item != kind] for kind in kinds]
    rng = random.Random(1)
    for _ in range(samples):
        keep = rng.uniform(0.3, 0.9)
        sets.append([item for item in pool if rng.random() < keep])
    return sets


def oracle_tricks(oracle: Path) -> list[str]:
    out = subprocess.run([str(oracle), "--dump-settings"], capture_output=True, text=True, encoding="utf-8",
                         check=True).stdout
    return [line.split("\t")[1] for line in out.splitlines() if line.startswith("Logical Tricks\t")]


def oracle_effect(oracle: Path, checks: Path, sets, baseline, compared, extra) -> set[str]:
    session = cl.OracleSession(oracle, checks, extra)
    try:
        effect: set[str] = set()
        for items, base in zip(sets, baseline):
            answer = session.query(items)
            if not isinstance(answer, str):
                effect |= (answer & compared) - base
        return effect
    finally:
        session.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--archipelago", type=Path, required=True)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=60)
    parser.add_argument("--oracle-set", action="append", default=[], metavar="NAME=VALUE",
                        help="a further 3D randomizer setting on both runs, e.g. 'Tokensanity=All Tokens'")
    parser.add_argument("--option", action="append", default=[], metavar="NAME=VALUE",
                        help="an apworld option on both runs, e.g. shuffle_skull_tokens=all")
    arguments = parser.parse_args()
    oracle = arguments.oracle.resolve()
    report_path = arguments.report.resolve()
    archipelago = arguments.archipelago.resolve()

    known_items = cl.oracle_items(oracle)
    known_locations = cl.oracle_locations(oracle)
    extra = dict(entry.split("=", 1) for entry in arguments.oracle_set)
    options = cl.parse_options(arguments.option)
    base_logic = cl.ApworldLogic(archipelago, options=options)
    pool = [name for name in base_logic.progression_pool() if name in known_items]
    sets = effect_sets(pool, arguments.samples)
    compared = {location.name for location in base_logic.locations if location.name in known_locations}
    checks = sorted(location.name for location in base_logic.unfixed if location.name in known_locations)
    apworld_base = [base_logic.reachable(items) & compared for items in sets]

    from worlds.oot3d.Enums import Tricks  # importable once ApworldLogic set the paths
    report = {"sets": len(sets), "soh": {}, "oot3d": {}, "pairs": []}
    for trick in Tricks:
        logic = cl.ApworldLogic(archipelago, options={**options, "tricks_in_logic": [str(trick)]})
        effect: set[str] = set()
        for items, base in zip(sets, apworld_base):
            effect |= (logic.reachable(items) & compared) - base
        report["soh"][str(trick)] = sorted(effect)
        print(f"soh {trick}: {len(effect)}", flush=True)

    with tempfile.TemporaryDirectory() as scratch:
        checks_file = Path(scratch) / "checks.txt"
        checks_file.write_text("".join(name + "\n" for name in checks), encoding="utf-8")
        session = cl.OracleSession(oracle, checks_file, extra)
        try:
            oracle_base = []
            for items in sets:
                answer = session.query(items)
                oracle_base.append(set() if isinstance(answer, str) else answer & compared)
        finally:
            session.close()
        for trick in oracle_tricks(oracle):
            effect = oracle_effect(oracle, checks_file, sets, oracle_base, compared, {**extra, trick: "Enabled"})
            report["oot3d"][trick] = sorted(effect)
            print(f"3d {trick}: {len(effect)}", flush=True)

    for trick, effect in report["oot3d"].items():
        best, score = None, 0.0
        for soh, soh_effect in report["soh"].items():
            union = set(effect) | set(soh_effect)
            if union:
                jaccard = len(set(effect) & set(soh_effect)) / len(union)
                if jaccard > score:
                    best, score = soh, jaccard
        report["pairs"].append({"oot3d": trick, "soh": best, "jaccard": round(score, 3),
                                "oot3d_effect": len(effect)})
    report_path.write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
    paired = sum(1 for pair in report["pairs"] if pair["jaccard"] >= 0.5)
    print(f"{paired} of {len(report['pairs'])} 3D tricks pair with a SoH trick at Jaccard >= 0.5")
    return 0


if __name__ == "__main__":
    sys.exit(main())
