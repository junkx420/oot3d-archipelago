"""Ocarina of Time 3D data for the tables imported from the SoH apworld.

data/locations.json and data/items.json are generated from OoT3D_Randomizer
by tools/oot3d/archipelago/recon/generate_tables.py; their ids are the ones
the TriAevum client reports. Enums.py carries the 3D display names
(recon/map_soh_names.py --apply), so a SoH table entry finds its 3D id by
name. Entries without a 3D counterpart drop out of the datapackage.
"""

from __future__ import annotations

import json
import pkgutil
from typing import Any

GAME = "Ocarina of Time 3D"


def _load(name: str, key: str) -> list[dict[str, Any]]:
    data = pkgutil.get_data(__name__, f"data/{name}")
    if data is None:
        raise FileNotFoundError(f"apworld data/{name} is missing")
    return json.loads(data.decode("utf-8"))[key]


LOCATIONS = _load("locations.json", "locations")
ITEMS = _load("items.json", "items")
# Vanilla (non Master Quest) locations: the apworld has no MQ logic yet.
LOCATION_IDS: dict[str, int] = {
    location["name"]: location["id"] for location in LOCATIONS if not location["master_quest"]}
ITEM_IDS: dict[str, int] = {item["name"]: item["id"] for item in ITEMS}
