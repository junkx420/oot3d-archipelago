"""Tests of the Ocarina of Time 3D world as players generate it.

The rule tests pin the places where the ported Ship of Harkinian rules were
brought in line with OoT3D_Randomizer's logic; recon/compare_logic.py found
each of them, and its minimal item sets are reused here.
"""

from Options import OptionError

from ..Enums import Items, Locations, Regions
from .. import oot3d_glitch_options, oot3d_glitched, oot3d_options
from .bases import SohTestBase


class TestOptionsCreator(SohTestBase):
    """The Options Creator and the template show only what a player can
    choose for the TriAevum client."""

    def test_only_choosable_options_are_shown(self) -> None:
        from Options import PerGameCommonOptions, Visibility
        common = set(PerGameCommonOptions.type_hints)
        shown = {name for name, option in self.world.options_dataclass.type_hints.items()
                 if name not in common and option.visibility & Visibility.simple_ui}
        self.assertEqual(shown, {"death_link", "item_pool", "start_inventory_from_pool", "shuffle_kokiri_sword",
                                 "small_key_shuffle", "boss_key_shuffle", "maps_and_compasses",
                                 "zoras_fountain", "jabu_jabu", "rainbow_bridge", "ganons_trials",
                                 "ganons_trials_count", "start_with_kokiri_sword", "start_with_deku_shield",
                                 "start_with_ocarina", "start_with_magic_beans",
                                 "closed_forest", "kakariko_gate", "door_of_time",
                                 "shuffle_songs", "shuffle_dungeon_rewards", "shuffle_skull_tokens", "skulls_sun_song",
                                 "shuffle_scrubs",
                                 "shuffle_shops", "shuffle_shops_item_amount", "shuffle_cows",
                                 "rainbow_bridge_stones_required", "rainbow_bridge_medallions_required",
                                 "rainbow_bridge_dungeon_rewards_required", "rainbow_bridge_dungeons_required",
                                 "rainbow_bridge_skull_tokens_required", "triforce_hunt", "triforce_hunt_pieces_total",
                                 "triforce_hunt_pieces_required_percentage", "starting_hearts", "shuffle_ocarinas",
                                 "shuffle_weird_egg", "shuffle_gerudo_membership_card",
                                 "tricks_in_logic", "enable_all_tricks"} | oot3d_glitch_options.OPTION_NAMES)

    def test_core_options_stay_visible_for_other_games(self) -> None:
        from Options import DeathLink, Visibility
        self.assertEqual(DeathLink.visibility, Visibility.all)

    def test_partial_choices_name_their_supported_values(self) -> None:
        doc = self.world.options_dataclass.type_hints["small_key_shuffle"].__doc__
        self.assertIn("supports: own_dungeon, vanilla", doc)


class TestSupportedOptions(SohTestBase):
    """Default options: what the TriAevum client supports. Setting the world
    up at all shows the defaults pass the client's option check (the world
    adjusts some options afterwards, so they are not checked again later)."""
    skip_client_option_check = False

    def test_unsupported_value_is_rejected(self) -> None:
        option = self.world.options.door_of_time
        original = option.value
        try:
            option.value = option.options["song_only"]
            with self.assertRaises(OptionError):
                oot3d_options.validate(self.world.options, "Tester")
        finally:
            option.value = original

    def test_links_pocket_comes_after_time_travel(self) -> None:
        self.assertFalse(self.can_reach_location(Locations.LINKS_POCKET))

    def test_lake_hylia_sun_has_no_scarecrow_route(self) -> None:
        # Enough for the longshot and scarecrow route, not for the Water Temple.
        self.collect_by_name([Items.BOOMERANG, Items.PROGRESSIVE_HOOKSHOT, Items.PROGRESSIVE_HOOKSHOT,
                              Items.PROGRESSIVE_BOMB_BAG, Items.PROGRESSIVE_BOW, Items.PROGRESSIVE_SLINGSHOT,
                              Items.BOTTLE_WITH_RUTOS_LETTER])
        self.assertFalse(self.can_reach_location(Locations.LH_SUN))

    def test_child_leaves_the_forest_only_after_the_deku_tree(self) -> None:
        # Silver scale and bombs open the Lost Woods shortcuts to Zora's River
        # and Goron City, but child Link may not take them before clearing the
        # Deku Tree, which needs the slingshot.
        self.collect_by_name([Items.PROGRESSIVE_SCALE, Items.PROGRESSIVE_SCALE, Items.PROGRESSIVE_BOMB_BAG])
        self.assertFalse(self.can_reach_location(Locations.ZR_OPEN_GROTTO_CHEST))
        self.collect_by_name([Items.PROGRESSIVE_SLINGSHOT])
        self.assertTrue(self.can_reach_location(Locations.ZR_OPEN_GROTTO_CHEST))

    def test_child_cannot_walk_into_the_kakariko_backyard(self) -> None:
        self.collect_by_name([Items.PROGRESSIVE_SLINGSHOT])
        self.assertFalse(self.can_reach_location(Locations.KAK_OPEN_GROTTO_CHEST))

    def test_child_spirit_torches_need_fire_for_the_anubis(self) -> None:
        self.collect_by_name([Items.BOOMERANG, Items.LENS_OF_TRUTH, Items.PROGRESSIVE_HOOKSHOT,
                              Items.PROGRESSIVE_HOOKSHOT, Items.PROGRESSIVE_BOMB_BAG, Items.PROGRESSIVE_SLINGSHOT,
                              Items.PROGRESSIVE_MAGIC_METER, Items.BOTTLE_WITH_RUTOS_LETTER])
        self.assertFalse(self.can_reach_location(Locations.SPIRIT_TEMPLE_CHILD_EARLY_TORCHES_CHEST))
        self.collect_by_name([Items.DINS_FIRE])
        self.assertTrue(self.can_reach_location(Locations.SPIRIT_TEMPLE_CHILD_EARLY_TORCHES_CHEST))


class TestWorldSettings(SohTestBase):
    """World settings the client sets as save flags, and items it is given at
    the start, generate and reach the client through slot data."""
    skip_client_option_check = False
    options = {
        "zoras_fountain": "open",
        "jabu_jabu": "open",
        "rainbow_bridge": "always_open",
        "ganons_trials": "set_number",
        "ganons_trials_count": 2,
        "start_with_kokiri_sword": True,
        "start_with_ocarina": "fairy_ocarina",
        "closed_forest": "off",
        "kakariko_gate": "open",
        "door_of_time": "open",
    }

    def test_settings_reach_the_world(self) -> None:
        # fill_slot_data sends these values (and ganons_trials as
        # required_trials); it needs a filled world, so a generated seed
        # checks the slot data itself.
        self.assertEqual(self.world.options.zoras_fountain.value, 2)
        self.assertEqual(self.world.options.jabu_jabu.value, 1)
        self.assertEqual(self.world.options.rainbow_bridge.value, 1)
        self.assertEqual(len(self.world.ganons_trials), 2)

    def test_starting_items_are_precollected(self) -> None:
        names = {item.name for item in self.multiworld.precollected_items[self.player]}
        self.assertIn(Items.KOKIRI_SWORD, names)
        self.assertIn(Items.PROGRESSIVE_OCARINA, names)

    def test_fortress_carpenters_stay_normal(self) -> None:
        option = self.world.options.fortress_carpenters
        original = option.value
        try:
            option.value = option.options["fast"]
            with self.assertRaises(OptionError):
                oot3d_options.validate(self.world.options, "Tester")
        finally:
            option.value = original


class TestGlitchedConditions(SohTestBase):
    """OoT3D_Randomizer's glitched conditions parse and build."""

    def test_every_condition_parses(self) -> None:
        unreadable = []
        for entry in oot3d_glitched.RULES:
            try:
                oot3d_glitched.parse(entry["rule"])
            except oot3d_glitched.Unsupported as reason:
                unreadable.append((entry["target"], str(reason)))
        self.assertEqual(unreadable, [])

    def test_glitchless_seeds_take_none(self) -> None:
        self.assertIsNone(self.world.glitched_logic)

    def test_options_match_the_3d_settings(self) -> None:
        self.assertEqual(set(oot3d_glitch_options.FIELDS),
                         set(oot3d_glitch_options.CATEGORIES) | set(oot3d_glitch_options.MISC))
        self.assertEqual(set(oot3d_glitched.GLITCH_SETTINGS.values()), set(oot3d_glitch_options.CATEGORIES))


class TestGlitchedLogic(SohTestBase):
    """With glitched logic, a chosen glitch opens its route: a ledge cancel
    gets child Link past King Zora without Ruto's letter."""
    options = {"logic_rules": "glitched", "glitch_ledge_cancel": "novice", "closed_forest": "off"}

    def test_most_conditions_are_used(self) -> None:
        logic = self.world.glitched_logic
        self.assertIsNotNone(logic)
        # Left out: areas SoH splits differently, enemy randomizer helpers,
        # Master Quest; recon/verify_glitches.py checks what is used.
        self.assertLess(len(logic.skipped), len(oot3d_glitched.RULES) // 5)

    def test_ledge_cancel_passes_king_zora(self) -> None:
        self.collect_by_name([Items.PROGRESSIVE_BOMB_BAG])
        self.assertTrue(self.multiworld.state.can_reach_region(str(Regions.ZD_BEHIND_KING_ZORA), self.player))


class TestGlitchedLogicNeedsTheGlitch(SohTestBase):
    options = {"logic_rules": "glitched", "closed_forest": "off"}

    def test_no_ledge_cancel_no_route(self) -> None:
        self.collect_by_name([Items.PROGRESSIVE_BOMB_BAG])
        self.assertFalse(self.multiworld.state.can_reach_region(str(Regions.ZD_BEHIND_KING_ZORA), self.player))


class TestTricks(SohTestBase):
    """Tricks are chosen by their OoT3D_Randomizer names and count only through
    their verified counterparts."""
    skip_client_option_check = False
    options = {"tricks_in_logic": ["DT B1 Navigation w/o Slingshot"]}

    def test_only_verified_tricks_are_offered(self) -> None:
        from .. import oot3d_tricks
        option = self.world.options_dataclass.type_hints["tricks_in_logic"]
        valid = option.valid_keys
        self.assertEqual(set(valid), set(oot3d_tricks.VERIFIED_TRICKS.values()))
        self.assertEqual(set(option._valid_keys), set(valid))  # what generation checks
        self.assertNotIn("Deku B1 Skip", valid)
        with self.assertRaises(Exception):
            option.from_any(["Deku B1 Skip"]).verify_keys()

    def test_verified_and_unverified_tricks(self) -> None:
        from ..LogicHelpers import can_do_trick
        from ..Enums import Regions, Tricks
        bundle = (Regions.ROOT, self.world)
        # An unverified SoH trick is only Universal Tracker's glitched item.
        self.assertEqual(type(can_do_trick(Tricks.DEKU_B1_BOW_WEBS, bundle)).__name__, "Has")
        # A verified one also reads the option.
        self.assertNotEqual(type(can_do_trick(Tricks.DEKU_B1_SKIP, bundle)).__name__, "Has")
