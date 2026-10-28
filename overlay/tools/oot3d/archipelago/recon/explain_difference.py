#!/usr/bin/env python3
"""Explain why the apworld reaches a location the 3D logic does not.

Rebuilds compare_logic.py's item sets, takes the named one, drops items while
the location stays reachable in the apworld only, and prints the minimal set
with the spots on the way (the path of regions and the rule that opens it).

Run with Archipelago's Python:
  explain_difference.py --archipelago <checkout> --oracle <exe> --location NAME --set "random 53"
      [--option NAME=VALUE]... [--oracle-set NAME=VALUE]...
"""

from __future__ import annotations

import argparse
import random
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import compare_logic as cl  # noqa: E402
import verify_glitches as vg  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--archipelago", type=Path, required=True)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--location", required=True)
    parser.add_argument("--set", dest="item_set", required=True)
    parser.add_argument("--samples", type=int, default=60)
    parser.add_argument("--glitches", help="a verify_glitches.py run name (all, or an option name)")
    parser.add_argument("--option", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--oracle-set", action="append", default=[], metavar="NAME=VALUE")
    arguments = parser.parse_args()
    oracle = arguments.oracle.resolve()
    options = cl.parse_options(arguments.option)
    extra = dict(entry.split("=", 1) for entry in arguments.oracle_set)
    if arguments.glitches:
        if arguments.glitches == "all":
            glitch_options, glitch_extra = vg.settings(list(vg.CATEGORIES), list(vg.MISC))
        else:
            name = arguments.glitches
            glitch_options, glitch_extra = vg.settings([name] if name in vg.CATEGORIES else [],
                                                       [name] if name in vg.MISC else [])
        options, extra = {**glitch_options, **options}, {**glitch_extra, **extra}
    known_items = cl.oracle_items(oracle)
    known_locations = cl.oracle_locations(oracle)
    logic = cl.ApworldLogic(arguments.archipelago.resolve(), options=options)
    pool = [name for name in logic.progression_pool() if name in known_items]
    sets = dict(cl.item_sets(pool, arguments.samples, random.Random(1)))
    items = sets[arguments.item_set]
    checks = sorted(location.name for location in logic.unfixed if location.name in known_locations)
    with tempfile.TemporaryDirectory() as scratch:
        checks_file = Path(scratch) / "checks.txt"
        checks_file.write_text("".join(name + "\n" for name in checks), encoding="utf-8")
        session = cl.OracleSession(oracle, checks_file, extra)
        try:
            def differs(candidate: list[str]) -> bool:
                oracle_reach = session.query(candidate)
                return (arguments.location in logic.reachable(candidate)
                        and isinstance(oracle_reach, set) and arguments.location not in oracle_reach)
            if not differs(items):
                print("no difference with this set")
                return 1
            kept = cl.minimize(items, differs)
        finally:
            session.close()
    print("minimal items:", kept)
    if options.get("logic_rules") != "glitched":
        return 0
    # Which glitched conditions does it take? Drop halves of them while the
    # location stays reachable with these items.
    glitched = sys.modules[type(logic.world).__module__ + ".oot3d_glitched"]
    every = list(glitched.RULES)

    def reaches(rules: list[dict]) -> bool:
        glitched.RULES = rules
        try:
            return arguments.location in cl.ApworldLogic(arguments.archipelago.resolve(), options=options).reachable(kept)
        finally:
            glitched.RULES = every

    needed = list(every)
    chunk = len(needed) // 2
    while chunk >= 1:
        index = 0
        while index < len(needed):
            candidate = needed[:index] + needed[index + chunk:]
            if reaches(candidate):
                needed = candidate
            else:
                index += chunk
        chunk //= 2
    for entry in needed:
        print(f"needs: {entry['area']} -> {entry['target']} ({entry['kind']}): {entry['rule']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
