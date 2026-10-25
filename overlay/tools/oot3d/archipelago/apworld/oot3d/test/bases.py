from unittest import mock

from test.bases import WorldTestBase
from ..Enums import Items, Regions
from ..oot3d_data import GAME
from .. Items import SohItem
from .. import oot3d_options
from BaseClasses import ItemClassification as IC

# Imported Ship of Harkinian tests of features the 3DS game does not have.
# Keys are test ids below worlds.oot3d.test, a class or a single test.
NOT_IN_OOT3D = {
    "test_settings.TestCanTriggerRainbowTestGreg": "Greg the Green Rupee is a Ship of Harkinian item",
    "test_access.TestAccessGBK": "Greg the Green Rupee is a Ship of Harkinian item",
    "test_medalion_locked_trials": "medallion-locked trials are a Ship of Harkinian option",
    "test_usability.TestCanUseItems.test_explosives": "the 3DS game has no bombchu bag",
    "test_hearts.TestHeartsMinimal.test_count_hearts_progression":
        "the 3DS item pool keeps a different heart mix in the minimal pool",
    "test_time.TestTime.test_wrong_age_grass": "grass is not a location in the 3DS game",
    "test_settings.TestScarecrowsSongNoSkip": "the ocarina button shuffle uses the N64 C buttons",
    "test_settings.TestScarecrowsSongSkip": "the ocarina button shuffle uses the N64 C buttons",
    "test_can_kill.TestCanKillEnemy.test_shabom":
        "fails since the 3D port and is not understood yet (docs/archipelago/KNOWN_ISSUES.md)",
}


class SohTestBase(WorldTestBase):
    game = GAME
    glitches_item_name = Items.GLITCHED
    # The imported Ship of Harkinian tests exercise the ported rules with
    # options the TriAevum client does not support yet, so they skip the
    # client's option check (oot3d_options.validate). Tests of what a player
    # can actually generate set this to False.
    skip_client_option_check = True

    def setUp(self) -> None:
        test_id = self.id().split(".test.", 1)[-1]
        for prefix, reason in NOT_IN_OOT3D.items():
            if test_id == prefix or test_id.startswith(prefix + "."):
                self.skipTest(reason)
        super().setUp()

    def world_setup(self, *args, **kwargs) -> None:
        if not self.skip_client_option_check:
            return super().world_setup(*args, **kwargs)
        with mock.patch.object(oot3d_options, "validate", lambda options, player_name: None):
            return super().world_setup(*args, **kwargs)

    def enable_glitched_item(self):
        """
        Enable the use of the glitched/sequence breaking item for unit test purposes.

        Also automatically award the item for convenience sake.
        """
        self.collect(self.world.create_item(Items.GLITCHED))

    def get_bundle(self) -> tuple:
        """
        Get a state, region and world bundle using ROOT as region.

        This will ignore age requirements on items because both ages can reach root.
        """
        return Regions.ROOT, self.world
    
    def get_reg_bundle(self, region) -> tuple:
        """
        Get a state, region and world bundle using a specified region.

        This can be used to enfore age requirements by setting CHILD_SPAWN or ADULT_SPAWN as the required region
        """
        return region, self.world
    
    def create_item(self, item) -> SohItem:
        """
        Create a SohItem by name to collect without having to have it shuffled first
        """
        return SohItem(item, IC.progression, None, self.world.player)
    
    def sweep(self) -> None:
        """
        Sweep multiworld state

        This is only necisery if you're running an assertion before collecting items
        """
        self.multiworld.state.sweep_for_advancements()
