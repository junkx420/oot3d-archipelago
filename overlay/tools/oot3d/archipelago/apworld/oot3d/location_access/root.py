from ..LogicHelpers import *

if TYPE_CHECKING:
    from .. import SohWorld


class EventLocations(StrEnum):
    ROOT_AMMO_DROP = "Root Ammo Drop"
    ROOT_DEKU_SHIELD = "Root Deku Shield"
    ROOT_HYLIAN_SHIELD = "Root Hylian Shield"
    TRIFORCE_HUNT_COMPLETION = "Triforce Hunt Completion"


def set_region_rules(world: "SohWorld") -> None:
    # Root
    # Events
    if bool(world.options.triforce_hunt):
        add_events(Regions.ROOT, world, [
            (EventLocations.TRIFORCE_HUNT_COMPLETION, Events.GAME_COMPLETED, lambda bundle: CanWinTriforceHunt())
        ])
    # Locations
    # The unmodified game hands out Link's Pocket (Rauru's medallion) in the
    # Chamber of the Sages after the first trip forward in time, not at the
    # start as the randomizers do.
    add_locations(Regions.ROOT, world, [
        (Locations.LINKS_POCKET, lambda bundle: has_item(Events.TIME_TRAVEL, bundle))
    ])
    # Connections
    connect_regions(Regions.ROOT, world, [
        (Regions.ROOT_EXITS, lambda bundle: starting_age(
            bundle) | has_item(Events.TIME_TRAVEL, bundle))
    ])

    # Connection for Master Sword as you start with it when starting age is adult and MS is not shuffled.
    if (world.options.starting_age == "adult" and not world.options.shuffle_master_sword):
        # Connections
        connect_regions(Regions.ROOT, world, [
            (Regions.MASTER_SWORD_PEDESTAL, lambda bundle: True_())
        ])

    # Root Exits
    # Connections
    connect_regions(Regions.ROOT_EXITS, world, [
        (Regions.CHILD_SPAWN, lambda bundle: is_child(bundle)),
        (Regions.ADULT_SPAWN, lambda bundle: is_adult(bundle)),
        (Regions.MINUET_OF_FOREST_WARP, lambda bundle: True_()),
        (Regions.BOLERO_OF_FIRE_WARP, lambda bundle: True_()),
        (Regions.SERENADE_OF_WATER_WARP, lambda bundle: True_()),
        (Regions.NOCTURNE_OF_SHADOW_WARP, lambda bundle: True_()),
        (Regions.REQUIEM_OF_SPIRIT_WARP, lambda bundle: True_()),
        (Regions.PRELUDE_OF_LIGHT_WARP, lambda bundle: True_()),
    ])

    # Child Spawn
    # Connections
    connect_regions(Regions.CHILD_SPAWN, world, [
        (Regions.KF_LINKS_HOUSE, lambda bundle: True_())
    ])

    # Adult Spawn
    # Connections
    connect_regions(Regions.ADULT_SPAWN, world, [
        (Regions.TEMPLE_OF_TIME, lambda bundle: True_())
    ])

    # Minuet of Forest Warp
    # Connections
    connect_regions(Regions.MINUET_OF_FOREST_WARP, world, [
        (Regions.SACRED_FOREST_MEADOW, lambda bundle: can_use(
            Items.MINUET_OF_FOREST, bundle))
    ])

    # Bolero of Fire Warp
    # Connections
    connect_regions(Regions.BOLERO_OF_FIRE_WARP, world, [
        (Regions.DMC_CENTRAL_LOCAL, lambda bundle: can_use(
            Items.BOLERO_OF_FIRE, bundle) & can_leave_forest(bundle))
    ])

    # Serenade of Water Warp
    # Connections
    connect_regions(Regions.SERENADE_OF_WATER_WARP, world, [
        (Regions.LAKE_HYLIA, lambda bundle: can_use(
            Items.SERENADE_OF_WATER, bundle) & can_leave_forest(bundle))
    ])

    # Requiem of Spirit Warp
    # Connections
    connect_regions(Regions.REQUIEM_OF_SPIRIT_WARP, world, [
        (Regions.DESERT_COLOSSUS, lambda bundle: can_use(
            Items.REQUIEM_OF_SPIRIT, bundle) & can_leave_forest(bundle))
    ])

    # Nocturne of Shadow Warp
    # Connections
    connect_regions(Regions.NOCTURNE_OF_SHADOW_WARP, world, [
        (Regions.GRAVEYARD_WARP_PAD_REGION, lambda bundle: can_use(
            Items.NOCTURNE_OF_SHADOW, bundle) & can_leave_forest(bundle))
    ])

    # Prelude of Light Warp
    # Connections
    connect_regions(Regions.PRELUDE_OF_LIGHT_WARP, world, [
        (Regions.TEMPLE_OF_TIME, lambda bundle: can_use(
            Items.PRELUDE_OF_LIGHT, bundle) & can_leave_forest(bundle))
    ])
