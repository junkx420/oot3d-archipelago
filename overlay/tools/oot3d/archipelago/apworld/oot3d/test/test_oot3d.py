"""Tests of the Ocarina of Time 3D world as players generate it.

The rule tests pin the places where the ported Ship of Harkinian rules were
brought in line with OoT3D_Randomizer's logic; recon/compare_logic.py found
each of them, and its minimal item sets are reused here.
"""

from Options import OptionError

from ..Enums import Items, Locations
from .. import oot3d_options
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
                                 "small_key_shuffle", "boss_key_shuffle", "maps_and_compasses"})

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
        option = self.world.options.closed_forest
        original = option.value
        try:
            option.value = option.options["off"]
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
