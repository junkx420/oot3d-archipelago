"""Option values the TriAevum client supports, version 1.

The imported SoH options describe Ship of Harkinian's game changes. A value
is allowed here only when the unmodified 3DS game already behaves that way,
when it needs no more from the client than replacing what a location gives
(chests, keys and dungeon items inside their own dungeon), when the client
sets it as save flags the way OoT3D_Randomizer does for a new save (Jabu-Jabu,
King Zora, the rainbow bridge, Ganon's trials, the Door of Time, the Kakariko
gate, Mido: ap_world_state.h) or in a hooked check (the forest exit), or when it
is an item the server gives at the start (start_with_*). Everything else
waits until the client implements it.

Defaults are set to the supported value, and a player who asks for an
unsupported value gets an OptionError naming it, never a silent override.
"""

from __future__ import annotations

from typing import Any

from Options import Choice, OptionError, PerGameCommonOptions, Toggle, Visibility

# option name -> (default, allowed values). Choice and Toggle values are the
# option's internal names; Range values are numbers.
SUPPORTED: dict[str, tuple[Any, frozenset]] = {
    # The 3DS game as it is.
    "closed_forest": ("on", frozenset({"on", "deku_only", "off"})),
    "kakariko_gate": ("closed", frozenset({"closed", "open"})),
    "door_of_time": ("closed", frozenset({"closed", "open"})),
    "zoras_fountain": ("closed", frozenset({"closed", "open"})),
    "sleeping_waterfall": ("closed", frozenset({"closed"})),
    "jabu_jabu": ("closed", frozenset({"closed", "open"})),
    "lock_overworld_doors": (False, frozenset({False})),
    "fortress_carpenters": ("normal", frozenset({"normal"})),
    "rainbow_bridge": ("vanilla", frozenset({"vanilla", "always_open", "stones", "medallions",
                                             "dungeon_rewards", "dungeons", "tokens"})),
    # The game has 3 stones, 6 medallions, 8 dungeons with a boss; Greg is not in it.
    "rainbow_bridge_stones_required": (3, frozenset(range(1, 4))),
    "rainbow_bridge_medallions_required": (6, frozenset(range(1, 7))),
    "rainbow_bridge_dungeon_rewards_required": (9, frozenset(range(1, 10))),
    "rainbow_bridge_dungeons_required": (8, frozenset(range(1, 9))),
    "rainbow_bridge_skull_tokens_required": (50, frozenset(range(1, 101))),
    "rainbow_bridge_greg_modifier": ("off", frozenset({"off"})),
    "ganons_trials": ("set_number", frozenset({"set_number", "skip"})),
    "ganons_trials_count": (6, frozenset(range(7))),
    "triforce_hunt": (False, frozenset({False, True})),
    "triforce_hunt_pieces_total": (30, frozenset(range(1, 101))),
    "triforce_hunt_pieces_required_percentage": (60, frozenset(range(1, 101))),
    "ganons_castle_boss_key": ("vanilla", frozenset({"vanilla"})),
    "ganons_castle_boss_key_greg_modifier": ("off", frozenset({"off"})),
    "big_poe_target_count": (10, frozenset({10})),
    "skip_child_zelda": (False, frozenset({False})),
    "skip_epona_race": (False, frozenset({False})),
    "complete_mask_quest": (False, frozenset({False})),
    "skip_scarecrows_song": (False, frozenset({False})),
    "start_with_links_pocket": ("nothing", frozenset({"nothing"})),
    "starting_age": ("child", frozenset({"child"})),
    "starting_hearts": (3, frozenset(range(1, 21))),
    "full_wallets": (False, frozenset({False})),
    "bombchu_bag": ("none", frozenset({"none"})),
    "bombchu_drops": (False, frozenset({False})),
    "blue_fire_arrows": (False, frozenset({False})),
    "sunlight_arrows": (False, frozenset({False})),
    "rocs_feather": (False, frozenset({False})),
    "infinite_upgrades": ("off", frozenset({"off"})),
    "skeleton_key": (False, frozenset({False})),
    "slingbow_break_beehives": (False, frozenset({False})),
    "medallion_locked_trials": (False, frozenset({False})),
    "ice_trap_count": (0, frozenset({0})),
    "ice_trap_filler_replacement": (0, frozenset({0})),
    "true_no_logic": (False, frozenset({False})),
    # Tricks count through their verified OoT3D_Randomizer counterparts only
    # (oot3d_tricks.py); enabling all enables those.
    "enable_all_tricks": (False, frozenset({False, True})),
    # Shuffles that only need a location's item replaced in game.
    "shuffle_kokiri_sword": (False, frozenset({False, True})),
    "small_key_shuffle": ("own_dungeon", frozenset({"vanilla", "own_dungeon"})),
    "boss_key_shuffle": ("own_dungeon", frozenset({"vanilla", "own_dungeon"})),
    "maps_and_compasses": ("own_dungeon", frozenset({"vanilla", "own_dungeon"})),
    "gerudo_fortress_key_shuffle": ("vanilla", frozenset({"vanilla"})),
    "key_rings": ("off", frozenset({"off"})),
    # Everything else stays as in the game.
    "shuffle_songs": ("off", frozenset({"off"})),
    "shuffle_skull_tokens": ("off", frozenset({"off", "dungeon", "overworld", "all"})),
    "skulls_sun_song": (False, frozenset({False, True})),
    "shuffle_master_sword": (False, frozenset({False})),
    "shuffle_childs_wallet": (False, frozenset({False})),
    "shuffle_tycoon_wallet": (False, frozenset({False})),
    "shuffle_ocarinas": (False, frozenset({False, True})),
    "shuffle_ocarina_buttons": (False, frozenset({False})),
    "shuffle_swim": (False, frozenset({False})),
    "shuffle_weird_egg": (False, frozenset({False, True})),
    "shuffle_gerudo_membership_card": (False, frozenset({False, True})),
    "shuffle_fishing_pole": (False, frozenset({False})),
    "shuffle_deku_stick_bag": (False, frozenset({False})),
    "shuffle_deku_nut_bag": (False, frozenset({False})),
    "shuffle_freestanding_items": ("off", frozenset({"off"})),
    "shuffle_shops": (False, frozenset({False, True})),
    "shuffle_shops_item_amount": (4, frozenset(range(8))),
    "shuffle_fish": ("off", frozenset({"off"})),
    "shuffle_scrubs": ("off", frozenset({"off", "one_time_only", "all"})),
    "shuffle_beehives": (False, frozenset({False})),
    "shuffle_cows": (False, frozenset({False, True})),
    "shuffle_pots": ("off", frozenset({"off"})),
    "shuffle_crates": ("off", frozenset({"off"})),
    "shuffle_trees": (False, frozenset({False})),
    "shuffle_merchants": ("off", frozenset({"off"})),
    "shuffle_frog_song_rupees": (False, frozenset({False})),
    "shuffle_adult_trade_items": (False, frozenset({False})),
    "shuffle_boss_souls": ("off", frozenset({"off"})),
    "shuffle_fountain_fairies": (False, frozenset({False})),
    "shuffle_stone_fairies": (False, frozenset({False})),
    "shuffle_bean_fairies": (False, frozenset({False})),
    "shuffle_song_fairies": (False, frozenset({False})),
    "shuffle_grass": ("off", frozenset({"off"})),
    "shuffle_dungeon_rewards": ("off", frozenset({"off"})),
    "shuffle_100_gs_reward": (False, frozenset({False})),
    "start_with_kokiri_sword": (False, frozenset({False, True})),
    "start_with_master_sword": (False, frozenset({False})),
    "start_with_deku_shield": (False, frozenset({False, True})),
    "start_with_ocarina": ("off", frozenset({"off", "fairy_ocarina", "ocarina_of_time"})),
    "start_with_stick_ammo": (False, frozenset({False})),
    "start_with_nut_ammo": (False, frozenset({False})),
    "start_with_magic_beans": (False, frozenset({False, True})),
    "start_with_zeldas_lullaby": (False, frozenset({False})),
    "start_with_eponas_song": (False, frozenset({False})),
    "start_with_sarias_song": (False, frozenset({False})),
    "start_with_suns_song": (False, frozenset({False})),
    "start_with_song_of_time": (False, frozenset({False})),
    "start_with_song_of_storms": (False, frozenset({False})),
    "start_with_minuet": (False, frozenset({False})),
    "start_with_bolero": (False, frozenset({False})),
    "start_with_serenade": (False, frozenset({False})),
    "start_with_requiem": (False, frozenset({False})),
    "start_with_nocturne": (False, frozenset({False})),
    "start_with_prelude": (False, frozenset({False})),
}


def _option_value(option: Any) -> Any:
    """The comparable value: a Toggle's bool, a Choice's name, a Range's int.
    SoH Choices keep "false"/"true" aliases; they compare as bools."""
    if isinstance(option, Toggle):
        return bool(option.value)
    if isinstance(option, Choice):
        key = option.current_key
        return key == "true" if key in ("true", "false") else key
    return option.value


def apply_defaults(options_dataclass: type) -> None:
    """Make the supported value the default of every restricted option."""
    import typing
    hints = typing.get_type_hints(options_dataclass)
    for name, (default, _) in SUPPORTED.items():
        option_class = hints[name]
        choices = getattr(option_class, "options", None)
        if isinstance(default, bool):
            option_class.default = int(default)
        elif isinstance(default, str):
            option_class.default = choices[default]
        else:
            option_class.default = default


# Shown besides Archipelago's common options and the SUPPORTED options with
# more than one value. They change what the seed holds, not what the client
# must do in the game.
SHOWN = frozenset({"death_link", "item_pool", "start_inventory_from_pool", "tricks_in_logic"})


def apply_visibility(options_dataclass: type) -> None:
    """Show only what a player can choose: Ship of Harkinian's other options
    (hints, shuffles the client lacks, single supported values) are hidden
    from the Options Creator, the web pages and the template yaml. Their
    values stay as apply_defaults set them."""
    import typing
    hints = typing.get_type_hints(options_dataclass)
    common = set(PerGameCommonOptions.type_hints)
    package = __name__.rsplit(".", 1)[0]
    for name, option_class in hints.items():
        # Archipelago's own option classes are shared with every game.
        if name in common or name in SHOWN or not option_class.__module__.startswith(package):
            continue
        allowed = SUPPORTED.get(name, (None, frozenset()))[1]
        if len(allowed) < 2:
            option_class.visibility = Visibility.none
            continue
        choices = getattr(option_class, "options", None)
        if choices is not None:
            every = {key == "true" if key in ("true", "false") else key for key in choices}
        else:
            every = set(range(getattr(option_class, "range_start", 0), getattr(option_class, "range_end", -1) + 1))
        if every <= set(allowed):
            continue  # every value works: nothing to note
        if choices is None and all(isinstance(value, int) for value in allowed):
            values = f"{min(allowed)} to {max(allowed)}"
        else:
            values = ", ".join(sorted(str(value).lower() for value in allowed))
        note = f"\n\nOcarina of Time 3D (TriAevum) supports: {values}."
        if note not in (option_class.__doc__ or ""):
            option_class.__doc__ = (option_class.__doc__ or "").rstrip() + note


def validate(options: Any, player_name: str) -> None:
    unsupported = []
    for name, (_, allowed) in SUPPORTED.items():
        value = _option_value(getattr(options, name))
        if value not in allowed:
            unsupported.append(f"{name}={value!r} (supported: {sorted(map(str, allowed))})")
    if unsupported:
        raise OptionError(f"Ocarina of Time 3D ({player_name}): the TriAevum client does not support "
                          + "; ".join(unsupported))
