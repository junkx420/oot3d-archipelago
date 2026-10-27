#!/usr/bin/env python3
"""Check each paired trick (map_tricks.py) with the full logic comparison.

For a pair, the apworld runs with the SoH trick (chosen by its 3D name,
oot3d_tricks.py) and the oracle with the 3D trick, over compare_logic.py's item sets. The pair is verified when no
location is reachable in the apworld but not in the 3D logic (the apworld may
be stricter). The same is done once without tricks, as the baseline.

Run with Archipelago's Python:
  verify_tricks.py --archipelago <checkout> --oracle <exe> --pairs tricks.json --report verified.json
"""

from __future__ import annotations

import argparse
import json
import random
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import compare_logic as cl  # noqa: E402


def check(archipelago, oracle, options, extra, samples, known_items, known_locations):
    logic = cl.ApworldLogic(archipelago, options=options)
    pool = [name for name in logic.progression_pool() if name in known_items]
    compared = {location.name for location in logic.locations if location.name in known_locations}
    checks = sorted(location.name for location in logic.unfixed if location.name in known_locations)
    sets = list(cl.item_sets(pool, samples, random.Random(1)))
    with tempfile.TemporaryDirectory() as scratch:
        checks_file = Path(scratch) / "checks.txt"
        checks_file.write_text("".join(name + "\n" for name in checks), encoding="utf-8")
        session = cl.OracleSession(oracle, checks_file, extra)
        try:
            differences = cl.compare(logic, session, sets, compared)
        finally:
            session.close()
    more = {name: entry["apworld_only"][1] for name, entry in differences.items() if "apworld_only" in entry}
    stricter = sorted(name for name, entry in differences.items() if "oracle_only" in entry)
    return more, stricter, len(sets)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--archipelago", type=Path, required=True)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--pairs", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=60)
    parser.add_argument("--min-jaccard", type=float, default=0.5)
    parser.add_argument("--oracle-set", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--option", action="append", default=[], metavar="NAME=VALUE")
    arguments = parser.parse_args()
    oracle = arguments.oracle.resolve()
    archipelago = arguments.archipelago.resolve()
    report_path = arguments.report.resolve()
    pairs = [pair for pair in json.loads(arguments.pairs.read_text(encoding="utf-8"))["pairs"]
             if pair["soh"] and pair["jaccard"] >= arguments.min_jaccard]
    extra = dict(entry.split("=", 1) for entry in arguments.oracle_set)
    options = cl.parse_options(arguments.option)
    known_items = cl.oracle_items(oracle)
    known_locations = cl.oracle_locations(oracle)

    report = {"baseline": {}, "pairs": []}
    more, stricter, count = check(archipelago, oracle, options, extra, arguments.samples, known_items,
                                  known_locations)
    report["baseline"] = {"more_permissive": more, "stricter": len(stricter), "item_sets": count}
    print(f"baseline: {len(more)} more permissive, {len(stricter)} stricter", flush=True)
    for pair in pairs:
        more, stricter, _ = check(archipelago, oracle, {**options, "tricks_in_logic": [pair["oot3d"]]},
                                  {**extra, pair["oot3d"]: "Enabled"}, arguments.samples, known_items,
                                  known_locations)
        new = {name: example for name, example in more.items() if name not in report["baseline"]["more_permissive"]}
        verified = not new
        report["pairs"].append({**pair, "verified": verified, "more_permissive": new, "stricter": len(stricter)})
        print(f"{'ok ' if verified else 'NO '} {pair['oot3d']} = {pair['soh']}"
              f"{'' if verified else ': ' + ', '.join(sorted(new))}", flush=True)
        report_path.write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
    report_path.write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
    print(f"{sum(pair['verified'] for pair in report['pairs'])} of {len(report['pairs'])} pairs verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
