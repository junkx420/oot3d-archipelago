"""Universal Tracker support: the world regenerated from a seed's slot_data.

Universal Tracker connects to the server beside the game, rebuilds the world
from slot_data (interpret_slot_data, then multiworld.re_gen_passthrough with
default options, ut_can_gen_without_yaml) and runs the generation steps up to
the fill. These tests do the same with a generated seed's slot_data after a
JSON round trip, as the server sends it, and compare the rebuilt world's
locations and logic with the original's.
"""

import json
import unittest
from argparse import Namespace

from BaseClasses import CollectionState, MultiWorld
from Fill import distribute_items_restrictive
from NetUtils import encode
from test.general import gen_steps, setup_solo_multiworld
from worlds.AutoWorld import AutoWorldRegister, call_all

from ..Enums import Items
from ..oot3d_data import GAME

# Universal Tracker stops before the fill.
TRACKER_STEPS = ("generate_early", "create_regions", "create_items", "set_rules", "connect_entrances",
                 "generate_basic")


def generate_seed(seed: int) -> tuple[MultiWorld, dict]:
    world_type = AutoWorldRegister.world_types[GAME]
    multiworld = setup_solo_multiworld(world_type, gen_steps, seed)
    distribute_items_restrictive(multiworld)
    call_all(multiworld, "post_fill")
    slot_data = multiworld.worlds[1].fill_slot_data()
    return multiworld, json.loads(encode(slot_data))


def regenerate(slot_data: dict) -> MultiWorld:
    world_type = AutoWorldRegister.world_types[GAME]
    multiworld = MultiWorld(1)
    multiworld.game = {1: GAME}
    multiworld.player_name = {1: "Tester1"}
    multiworld.set_seed(None)
    multiworld.re_gen_passthrough = {GAME: world_type.interpret_slot_data(slot_data)}
    args = Namespace()
    for key, option in world_type.options_dataclass.type_hints.items():
        setattr(args, key, {1: option.from_any(option.default)})
    multiworld.set_options(args)
    multiworld.state = CollectionState(multiworld)
    for step in TRACKER_STEPS:
        call_all(multiworld, step)
    return multiworld


def location_names(multiworld: MultiWorld) -> set[str]:
    return {location.name for location in multiworld.get_locations(1) if location.address is not None}


def reachable(multiworld: MultiWorld, items: list[str]) -> set[str]:
    state = CollectionState(multiworld)
    world = multiworld.worlds[1]
    for name in items:
        state.collect(world.create_item(str(name)), True)
    # Only events: the tracker knows the items the server sent, not what the
    # fill placed (the original world's filled locations would add theirs).
    state.sweep_for_advancements([location for location in multiworld.get_locations(1)
                                  if location.address is None])
    return {location.name for location in multiworld.get_locations(1)
            if location.address is not None and location.can_reach(state)}


class TestUniversalTracker(unittest.TestCase):
    def test_regenerated_world_matches_the_seed(self) -> None:
        original, slot_data = generate_seed(1234)
        rebuilt = regenerate(slot_data)
        self.assertTrue(rebuilt.worlds[1].using_ut)
        self.assertEqual(location_names(original), location_names(rebuilt))
        # Logic is the same: with nothing, and with a handful of key items.
        for items in ([], [Items.PROGRESSIVE_SLINGSHOT, Items.BOOMERANG, Items.PROGRESSIVE_BOMB_BAG,
                           Items.PROGRESSIVE_HOOKSHOT, Items.ZELDAS_LULLABY, Items.EPONAS_SONG,
                           Items.STRENGTH_UPGRADE]):
            with self.subTest(items=items):
                self.assertEqual(reachable(original, items), reachable(rebuilt, items))

    def test_no_glitched_locations_for_the_tracker(self) -> None:
        # The tracker lists "glitched" locations only for a world that names
        # a glitches item; Ship of Harkinian's N64 tricks do not apply to 3D.
        world_type = AutoWorldRegister.world_types[GAME]
        self.assertFalse(getattr(world_type, "glitches_item_name", ""))

    def test_slot_data_names_every_option_the_tracker_reads(self) -> None:
        _, slot_data = generate_seed(99)
        rebuilt = regenerate(slot_data)
        world = rebuilt.worlds[1]
        self.assertEqual(world.options.death_link.value, 0)
        self.assertEqual(world.options.closed_forest.current_key, "on")
