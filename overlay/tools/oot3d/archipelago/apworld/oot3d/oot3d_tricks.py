"""Logical tricks for Ocarina of Time 3D.

Ship of Harkinian's tricks describe the N64 game. A SoH trick counts in
logic here only through its OoT3D_Randomizer counterpart: the 3D trick with
the same effect (recon/map_tricks.py), checked with the full logic comparison
against the 3D randomizer's own rules (recon/verify_tricks.py: with both
tricks on, no location the apworld reaches is out of the 3D logic's reach).
Players choose tricks by their 3D randomizer names; a SoH trick without a
verified counterpart is never in logic.
"""

from __future__ import annotations

# SoH trick (Enums.Tricks value) -> OoT3D_Randomizer trick (its setting name).
VERIFIED_TRICKS: dict[str, str] = {
    "Damage Boost": "WaT Boss Ledge w/ Bombs",
    "DC Scarecrow GS": "DC Scarecrow GS w/ Armos Statue",
    "Deku B1 Skip": "DT B1 Navigation w/o Slingshot",
    "DMT Bombable": "DMT Wall Chest w/ Strength",
    "Fewer Tunic Requirements": "Fewer Tunic Requirements",
    "Fire Boss Door Jump": "FiT Boss Door w/o Hover Boots",
    "Fire Flame Maze": "FiT Firewall Maze w/ Nothing",
    "Fire Strength": "FiT Climb Block w/o Strength",
    "Flaming Chests": "Open chests through flame circles",
    "Forest Doorframe": "FoT East Scarecrow w/ Hover Boots",
    "GC Leftmost": "GoC Maze Left Chest w/ Hover Boots",
    "GC Pot Strength": "GoC Goron Vase PoH w/ Strength",
    "GC Rolling Strength": "GoC Child Goron w/ Strength",
    "Grottos Without Agony": "Grotto Access w/o Shard of Agony",
    "GTG Without Hookshot": "GTG West Silver Rupee w/o Hookshot",
    "GY PoH": "GY Crate PoH w/ Boomerang",
    "HW Reverse": "Haunted Wasteland in Reverse",
    "Kak Man On Roof": "Kak Roof Guy w/o Hookshot",
    "Lens BotW": "BotW Navigate w/o Lens of Truth",
    "Lens GTG": "GTG Navigate w/o Lens of Truth",
    "Lens HW": "Haunted Wasteland w/o Lens of Truth",
    "Lens Shadow": "ShT Early Rooms w/o Lens of Truth",
    "LH Lab Diving": "LH Lab Dive w/o Gold Scale",
    "LH Lab Wall GS": "LH Lab Wall GS w/ Jump Slash",
    "Pass Guards With Nothing": "GF Through Kitchen w/ Nothing",
    "Spirit Lobby GS": "SpT Main Room GS w/ Boomerang",
    "Spirit Map Chest": "SpT Map Chest w/ Bow",
    "Spirit Sun Chest": "SpT Sun Block Room w/ Bow",
    "Water BK Region": "WaT B1 North Area w/ Hover Boots",
    "Water FW Central GS": "WaT Center Room GS w/ Farore's Wind",
    "Water Hookshot Falling Platform GS": "WaT Waterfall GS w/ Hookshot",
    "Water Longshot Torch": "WaT Torch Longshot Shortcut",
    # TriAevum's own: OoT3D_Randomizer's rule for the basement vines GS.
    "Deku B1 Vines GS": "DT B1 Vines GS w/ Jump Slash",
}


def trick_names() -> list[str]:
    """The tricks a player can choose, by their 3D randomizer names."""
    return sorted(set(VERIFIED_TRICKS.values()))


def apply_tricks(options_dataclass: type) -> None:
    """Let tricks_in_logic offer the verified 3D tricks only."""
    import typing
    option_class = typing.get_type_hints(options_dataclass)["tricks_in_logic"]
    names = trick_names()
    # Archipelago freezes valid_keys into _valid_keys when the class is made;
    # verify_keys reads that one.
    option_class.valid_keys = frozenset(names)
    option_class._valid_keys = frozenset(names)
    option_class.__doc__ = (
        "Tricks the logic may expect, by their OoT3D_Randomizer names. Each one is checked against "
        "the 3D randomizer's own logic.\nTricks: " + (", ".join(names) or "none yet"))
