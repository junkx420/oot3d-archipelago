"""Glitch options for Ocarina of Time 3D, as OoT3D_Randomizer offers them.

With logic_rules set to glitched, the logic also takes OoT3D_Randomizer's
glitched routes (oot3d_glitched.py). Each glitch category has the difficulty
levels the 3D randomizer has for it; a route counts when the chosen level is
at least the route's. The miscellaneous glitches are single setups, on or off.
None of this changes the game: the client plays the same either way.
"""

from __future__ import annotations

from dataclasses import dataclass

from Options import Choice, OptionGroup, Toggle

DIFFICULTIES = ("novice", "intermediate", "advanced", "expert", "hero")


class LogicRules(Choice):
    """Glitchless: the logic expects no glitches.
    Glitched: the logic may also expect the glitches chosen below, at their difficulty
    (OoT3D_Randomizer's glitched logic). A glitch left disabled is never expected."""
    display_name = "Logic Rules"
    option_glitchless = 0
    option_glitched = 1
    default = 0


def _category(class_name: str, display_name: str, mask: int, doc: str) -> type:
    """A glitch category: disabled plus the difficulties in mask (bit 0 is
    novice). The values are the 3D randomizer's levels (disabled 0, novice 1,
    ... hero 5), so a level compares with a route's difficulty directly."""
    attributes = {"__doc__": doc + "\nOnly counts with Logic Rules set to glitched.",
                  "display_name": display_name, "option_disabled": 0, "default": 0,
                  "__module__": __name__}
    for bit, name in enumerate(DIFFICULTIES):
        if mask >> bit & 1:
            attributes["option_" + name] = bit + 1
    return type(class_name, (Choice,), attributes)


def _misc(class_name: str, display_name: str, doc: str) -> type:
    return type(class_name, (Toggle,), {"__doc__": doc + "\nOnly counts with Logic Rules set to glitched.",
                                        "display_name": display_name, "__module__": __name__})


GlitchRestrictedItems = _category("GlitchRestrictedItems", "Glitch: Restricted Items", 0b00001,
                                  "Using items where the game normally forbids them.")
GlitchSuperStab = _category("GlitchSuperStab", "Glitch: Super Stab", 0b00001,
                            "Deku stick stabs that hit through walls while shielding.")
GlitchISG = _category("GlitchISG", "Glitch: Infinite Sword Glitch", 0b00111,
                      "Keeping the sword hitbox active (ISG), needed for bomb hovers.")
GlitchHover = _category("GlitchHover", "Glitch: Bomb Hover", 0b00111,
                        "Hovering with bombs or bombchus under ISG.")
GlitchBombOI = _category("GlitchBombOI", "Glitch: Ocarina Items (Bomb)", 0b01111,
                         "Getting an item from an ocarina cutscene setup with a bomb.")
GlitchHoverBoost = _category("GlitchHoverBoost", "Glitch: Hover Boost", 0b00111,
                             "Gaining speed with hover boots and a bomb.")
GlitchSuperSlide = _category("GlitchSuperSlide", "Glitch: Extended Super Slide", 0b01111,
                             "Sliding far with the speed of a backflip and a bomb.")
GlitchMegaflip = _category("GlitchMegaflip", "Glitch: Megaflip", 0b11111,
                           "A shield-cancelled backflip boosted by an explosion.")
GlitchASlide = _category("GlitchASlide", "Glitch: A-Slide", 0b01111,
                         "Child Link's shield slide boosted by an explosion.")
GlitchHammerSlide = _category("GlitchHammerSlide", "Glitch: Hammer Slide", 0b00011,
                              "Sliding with the hammer and hover boots.")
GlitchLedgeCancel = _category("GlitchLedgeCancel", "Glitch: Ledge Cancel", 0b00111,
                              "Cancelling a ledge grab with an explosion.")
GlitchActionSwap = _category("GlitchActionSwap", "Glitch: Action Swap", 0b00101,
                             "Swapping the held item during an action.")
GlitchQPA = _category("GlitchQPA", "Glitch: Quick Put Away", 0b01111,
                      "Putting a deku stick away at the right moment to keep its hitbox.")
GlitchHookshotClip = _category("GlitchHookshotClip", "Glitch: Hookshot Clip", 0b00011,
                               "Clipping through walls with the hookshot.")
GlitchHookshotJump_Bonk = _category("GlitchHookshotJump_Bonk", "Glitch: Hookshot Jump (Bonk)", 0b00111,
                                    "Gaining height with the hookshot and a bonk.")
GlitchHookshotJump_Boots = _category("GlitchHookshotJump_Boots", "Glitch: Hookshot Jump (Boots)", 0b00111,
                                     "Gaining height with the hookshot and a boots swap.")
GlitchCutsceneDive = _category("GlitchCutsceneDive", "Glitch: Cutscene Dives", 0b00111,
                               "Diving into water during a cutscene.")
GlitchNaviDive_Stick = _category("GlitchNaviDive_Stick", "Glitch: Navi Dive (Stick)", 0b00111,
                                 "Navi dives with a deku stick, without a triple slash clip.")
GlitchTripleSlashClip = _category("GlitchTripleSlashClip", "Glitch: Triple Slash Clip", 0b01111,
                                  "Clipping through walls with a sword combo.")
GlitchLedgeClip = _category("GlitchLedgeClip", "Glitch: Ledge Clip", 0b00111,
                            "Adult Link clipping through ledges.")
GlitchSeamWalk = _category("GlitchSeamWalk", "Glitch: Seam Walk", 0b11111,
                           "Walking up seams between walls.")

GlitchWWTEscape = _misc("GlitchWWTEscape", "Glitch: WWT Kokiri Forest Escape",
                        "Leaving the forest with a wrong warp from the Kokiri Forest.")
GlitchGVTentAsChild = _misc("GlitchGVTentAsChild", "Glitch: Enter GV Tent as Child",
                            "Child Link enters the carpenters' tent in Gerudo Valley.")
GlitchGFGuardSneak = _misc("GlitchGFGuardSneak", "Glitch: Sneak Past the GF Guard",
                           "Getting past the Gerudo Fortress gate guard.")
GlitchItemlessWasteland = _misc("GlitchItemlessWasteland", "Glitch: Cross the HW without Items",
                                "Crossing the Haunted Wasteland without items.")
GlitchOccamsStatue = _misc("GlitchOccamsStatue", "Glitch: Occam's Statue",
                           "The Occam's statue setup.")
GlitchZDOoBJumpSlash = _misc("GlitchZDOoBJumpSlash", "Glitch: ZD OoB with Jump Slash",
                             "Leaving the bounds of Zora's Domain with a jump slash.")
GlitchJabuStickRecoil = _misc("GlitchJabuStickRecoil", "Glitch: Enter Jabu without Bottle",
                              "Entering Jabu-Jabu with deku stick recoil.")
GlitchJabuAdult = _misc("GlitchJabuAdult", "Glitch: Enter Jabu as Adult",
                        "Adult Link enters Jabu-Jabu.")
GlitchBlueFireWall = _misc("GlitchBlueFireWall", "Glitch: Break Walls with Blue Fire",
                           "Breaking bombable walls with blue fire.")
GlitchClassicHalfie = _misc("GlitchClassicHalfie", "Glitch: Classic Halfie",
                            "The classic half-heart setup.")
GlitchModernHalfie = _misc("GlitchModernHalfie", "Glitch: Modern Halfie",
                           "The modern half-heart setup.")
GlitchJabuSwitch = _misc("GlitchJabuSwitch", "Glitch: Jabu Switch with CS Item",
                         "Pressing a Jabu-Jabu switch with a cutscene item.")
GlitchForestBKSkip = _misc("GlitchForestBKSkip", "Glitch: Forest Temple BK Skip",
                           "Skipping the Forest Temple boss key door.")
GlitchFireGrunzClip = _misc("GlitchFireGrunzClip", "Glitch: Fire Temple Grunz Clip",
                            "The Grunz clip in the Fire Temple.")

# OoT3D_Randomizer's setting variable -> option class. CanDoGlitch's glitch
# types name these categories (the bomb ocarina item variants share one).
CATEGORIES: dict[str, type] = {
    "GlitchRestrictedItems": GlitchRestrictedItems, "GlitchSuperStab": GlitchSuperStab, "GlitchISG": GlitchISG,
    "GlitchHover": GlitchHover, "GlitchBombOI": GlitchBombOI, "GlitchHoverBoost": GlitchHoverBoost,
    "GlitchSuperSlide": GlitchSuperSlide, "GlitchMegaflip": GlitchMegaflip, "GlitchASlide": GlitchASlide,
    "GlitchHammerSlide": GlitchHammerSlide, "GlitchLedgeCancel": GlitchLedgeCancel,
    "GlitchActionSwap": GlitchActionSwap, "GlitchQPA": GlitchQPA, "GlitchHookshotClip": GlitchHookshotClip,
    "GlitchHookshotJump_Bonk": GlitchHookshotJump_Bonk, "GlitchHookshotJump_Boots": GlitchHookshotJump_Boots,
    "GlitchCutsceneDive": GlitchCutsceneDive, "GlitchNaviDive_Stick": GlitchNaviDive_Stick,
    "GlitchTripleSlashClip": GlitchTripleSlashClip, "GlitchLedgeClip": GlitchLedgeClip,
    "GlitchSeamWalk": GlitchSeamWalk,
}
MISC: dict[str, type] = {
    "GlitchWWTEscape": GlitchWWTEscape, "GlitchGVTentAsChild": GlitchGVTentAsChild,
    "GlitchGFGuardSneak": GlitchGFGuardSneak, "GlitchItemlessWasteland": GlitchItemlessWasteland,
    "GlitchOccamsStatue": GlitchOccamsStatue, "GlitchZDOoBJumpSlash": GlitchZDOoBJumpSlash,
    "GlitchJabuStickRecoil": GlitchJabuStickRecoil, "GlitchJabuAdult": GlitchJabuAdult,
    "GlitchBlueFireWall": GlitchBlueFireWall, "GlitchClassicHalfie": GlitchClassicHalfie,
    "GlitchModernHalfie": GlitchModernHalfie, "GlitchJabuSwitch": GlitchJabuSwitch,
    "GlitchForestBKSkip": GlitchForestBKSkip, "GlitchFireGrunzClip": GlitchFireGrunzClip,
}


@dataclass
class GlitchOptions:
    logic_rules: LogicRules
    glitch_restricted_items: GlitchRestrictedItems
    glitch_super_stab: GlitchSuperStab
    glitch_isg: GlitchISG
    glitch_hover: GlitchHover
    glitch_bomb_oi: GlitchBombOI
    glitch_hover_boost: GlitchHoverBoost
    glitch_super_slide: GlitchSuperSlide
    glitch_megaflip: GlitchMegaflip
    glitch_a_slide: GlitchASlide
    glitch_hammer_slide: GlitchHammerSlide
    glitch_ledge_cancel: GlitchLedgeCancel
    glitch_action_swap: GlitchActionSwap
    glitch_qpa: GlitchQPA
    glitch_hookshot_clip: GlitchHookshotClip
    glitch_hookshot_jump_bonk: GlitchHookshotJump_Bonk
    glitch_hookshot_jump_boots: GlitchHookshotJump_Boots
    glitch_cutscene_dive: GlitchCutsceneDive
    glitch_navi_dive_stick: GlitchNaviDive_Stick
    glitch_triple_slash_clip: GlitchTripleSlashClip
    glitch_ledge_clip: GlitchLedgeClip
    glitch_seam_walk: GlitchSeamWalk
    glitch_wwt_escape: GlitchWWTEscape
    glitch_gv_tent_as_child: GlitchGVTentAsChild
    glitch_gf_guard_sneak: GlitchGFGuardSneak
    glitch_itemless_wasteland: GlitchItemlessWasteland
    glitch_occams_statue: GlitchOccamsStatue
    glitch_zd_oob_jump_slash: GlitchZDOoBJumpSlash
    glitch_jabu_stick_recoil: GlitchJabuStickRecoil
    glitch_jabu_adult: GlitchJabuAdult
    glitch_blue_fire_wall: GlitchBlueFireWall
    glitch_classic_halfie: GlitchClassicHalfie
    glitch_modern_halfie: GlitchModernHalfie
    glitch_jabu_switch: GlitchJabuSwitch
    glitch_forest_bk_skip: GlitchForestBKSkip
    glitch_fire_grunz_clip: GlitchFireGrunzClip


OPTION_NAMES = frozenset(GlitchOptions.__annotations__)
# The 3D glitch setting variable (the option class's name) -> its option field.
FIELDS: dict[str, str] = {option.__name__: name for name, option in
                          ((name, globals()[hint]) for name, hint in GlitchOptions.__annotations__.items())
                          if name != "logic_rules"}

glitch_option_group = OptionGroup("Glitches", [LogicRules, *CATEGORIES.values(), *MISC.values()], start_collapsed=True)
