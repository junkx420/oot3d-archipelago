"""OoT3D_Randomizer's glitched logic, applied to the ported region rules.

OoT3D_Randomizer gives some exits, locations and events a second condition,
marked /*Glitched*/; with glitched logic an entry is open when its glitchless
or its glitched condition holds (Entrance::GetConditionsMet). The conditions
are read from the 3D randomizer's source into data/glitched_logic.json
(recon/extract_glitched.py) as C++ expressions. Here they are parsed and built
into rule builder rules with the apworld's helpers, then ORed into the
matching SoH entrance, location or event:

- an exit: the entrance between the same regions; when SoH has none, a new
  entrance carries the glitched condition alone;
- a location: its rule, with the 3D area's reachability when SoH places it
  in another region;
- an event: the SoH event location in the same region giving the same event.

A condition is built only when every name in it has a translation; anything
else (enemy randomizer helpers, settings the client does not support, names
without a SoH region) leaves that entry out, so the logic never expects more
than the 3D randomizer does. skipped() lists what was left out and why.
"""

from __future__ import annotations

import json
import pkgutil
import re
from typing import TYPE_CHECKING, Any, Callable

from rule_builder.rules import CanReachRegion, False_, Rule, True_

from .Enums import Events, Items, Locations, Regions, Tricks
from . import oot3d_glitch_options as glitch_options

if TYPE_CHECKING:
    from . import SohWorld

# 3D area names that differ from the SoH region's member name.
AREA_ALIASES: dict[str, str] = {
    "THE_LOST_WOODS": "LOST_WOODS",
    "ZORAS_RIVER": "ZORA_RIVER",
    "GF_ARCHERY_RANGE": "GF_HBA_RANGE",
    "KAK_IMPAS_HOUSE_NEAR_COW": "KAK_IMPAS_HOUSE_BACK",
    "GERUDO_TRAINING_GROUNDS_ENTRYWAY": "GERUDO_TRAINING_GROUND_ENTRYWAY",
    "GERUDO_TRAINING_GROUNDS_LOBBY": "GERUDO_TRAINING_GROUND_LOBBY",
    "GERUDO_TRAINING_GROUNDS_CENTRAL_MAZE": "GERUDO_TRAINING_GROUND_CENTRAL_MAZE",
    "GERUDO_TRAINING_GROUNDS_CENTRAL_MAZE_RIGHT": "GERUDO_TRAINING_GROUND_CENTRAL_MAZE_RIGHT",
    "GERUDO_TRAINING_GROUNDS_LAVA_ROOM": "GERUDO_TRAINING_GROUND_LAVA_ROOM",
    "GERUDO_TRAINING_GROUNDS_HAMMER_ROOM": "GERUDO_TRAINING_GROUND_HAMMER_ROOM",
    "GERUDO_TRAINING_GROUNDS_EYE_STATUE_LOWER": "GERUDO_TRAINING_GROUND_EYE_STATUE_LOWER",
    "GERUDO_TRAINING_GROUNDS_EYE_STATUE_UPPER": "GERUDO_TRAINING_GROUND_EYE_STATUE_UPPER",
    "GERUDO_TRAINING_GROUNDS_HEAVY_BLOCK_ROOM": "GERUDO_TRAINING_GROUND_HEAVY_BLOCK_ROOM",
    "GERUDO_TRAINING_GROUNDS_LIKE_LIKE_ROOM": "GERUDO_TRAINING_GROUND_LIKE_LIKE_ROOM",
}

# 3D event variable -> the SoH event item it stands for.
EVENTS: dict[str, str] = {
    "GoronCityChildFire": "GC Child Fire Lit",
    "GCWoodsWarpOpen": "GC Woods Warp Open",
    "GCDaruniasDoorOpenChild": "GC Darunias Door Opened as Child",
    "DekuTreeClear": Events.DEKU_TREE_COMPLETED,
    "DodongosCavernClear": Events.DODONGOS_CAVERN_COMPLETED,
    "FireTempleClear": Events.FIRE_TEMPLE_COMPLETED,
    "FireLoopSwitch": "Fire Temple Loop Hammer Switch Hit",
    "GossipStoneFairy": Events.CAN_ACCESS_FAIRIES,
    "FairyPond": Events.CAN_ACCESS_FAIRIES,
    "FairyPot": Events.CAN_ACCESS_FAIRIES,
    "BugShrub": Events.CAN_ACCESS_BUGS,
    "ChildScarecrow": Events.CHILD_SCARECROW_UNLOCKED,
    "AdultScarecrow": Events.ADULT_SCARECROW_UNLOCKED,
    "DrainWell": Events.DRAIN_WELL,
    "DampesWindmillAccess": Events.DAMPES_WINDMILL_ACCESS,
    "WaterTempleLow": "Water Level Low",
    "WaterTempleMiddle": "Water Level Middle",
    "WaterTempleHigh": "Water Level High",
}

# 3D item names as CanUse takes them -> SoH items.
ITEMS: dict[str, Items] = {
    "STICKS": Items.STICKS, "BOOMERANG": Items.BOOMERANG, "BOW": Items.FAIRY_BOW, "Bow": Items.FAIRY_BOW,
    "SLINGSHOT": Items.FAIRY_SLINGSHOT, "HOOKSHOT": Items.HOOKSHOT, "LONGSHOT": Items.LONGSHOT,
    "HOVER_BOOTS": Items.HOVER_BOOTS, "IRON_BOOTS": Items.IRON_BOOTS, "KOKIRI_SWORD": Items.KOKIRI_SWORD,
    "MASTER_SWORD": Items.MASTER_SWORD, "BIGGORON_SWORD": Items.BIGGORONS_SWORD,
    "MEGATON_HAMMER": Items.MEGATON_HAMMER, "LENS_OF_TRUTH": Items.LENS_OF_TRUTH,
    "DINS_FIRE": Items.DINS_FIRE, "FARORES_WIND": Items.FARORES_WIND, "NAYRUS_LOVE": Items.NAYRUS_LOVE,
    "FIRE_ARROWS": Items.FIRE_ARROW, "ICE_ARROWS": Items.ICE_ARROW, "LIGHT_ARROWS": Items.LIGHT_ARROW,
    "SILVER_GAUNTLETS": Items.SILVER_GAUNTLETS, "ZORA_TUNIC": Items.ZORA_TUNIC, "GORON_TUNIC": Items.GORON_TUNIC,
    "DEKU_SHIELD": Items.DEKU_SHIELD, "HYLIAN_SHIELD": Items.HYLIAN_SHIELD, "MIRROR_SHIELD": Items.MIRROR_SHIELD,
}

# 3D song variables (the song item with its buttons) -> SoH songs.
SONGS: dict[str, Items] = {
    "ZeldasLullaby": Items.ZELDAS_LULLABY, "SariasSong": Items.SARIAS_SONG, "SunsSong": Items.SUNS_SONG,
    "SongOfStorms": Items.SONG_OF_STORMS, "EponasSong": Items.EPONAS_SONG, "SongOfTime": Items.SONG_OF_TIME,
    "MinuetOfForest": Items.MINUET_OF_FOREST, "BoleroOfFire": Items.BOLERO_OF_FIRE,
    "SerenadeOfWater": Items.SERENADE_OF_WATER, "NocturneOfShadow": Items.NOCTURNE_OF_SHADOW,
    "RequiemOfSpirit": Items.REQUIEM_OF_SPIRIT, "PreludeOfLight": Items.PRELUDE_OF_LIGHT,
}

# 3D small key items -> SoH keys.
KEYS: dict[str, Items] = {
    "FOREST_TEMPLE": Items.FOREST_TEMPLE_SMALL_KEY, "FIRE_TEMPLE": Items.FIRE_TEMPLE_SMALL_KEY,
    "WATER_TEMPLE": Items.WATER_TEMPLE_SMALL_KEY, "SPIRIT_TEMPLE": Items.SPIRIT_TEMPLE_SMALL_KEY,
    "SHADOW_TEMPLE": Items.SHADOW_TEMPLE_SMALL_KEY, "BOTTOM_OF_THE_WELL": Items.BOTTOM_OF_THE_WELL_SMALL_KEY,
    "GERUDO_TRAINING_GROUNDS": Items.TRAINING_GROUND_SMALL_KEY, "GANONS_CASTLE": Items.GANONS_CASTLE_SMALL_KEY,
}

# 3D trick variables -> their OoT3D_Randomizer setting names (oot3d_tricks.py
# offers the verified ones; another one is never on).
TRICKS: dict[str, str] = {
    "LogicFireStrength": "FiT Climb Block w/o Strength",
    "LogicManOnRoof": "Kak Roof Guy w/o Hookshot",
    "LogicFewerTunicRequirements": "Fewer Tunic Requirements",
    "LogicGrottosWithoutAgony": "Grotto Access w/o Shard of Agony",
    "LogicLensGtg": "GTG Navigate w/o Lens of Truth",
    "LogicGerudoKitchen": "GF Through Kitchen w/ Nothing",
    "LogicWindmillPoHHookshot": "Kak Windmill PoH w/ Hookshot",
    "LogicLensShadow": "ShT Early Rooms w/o Lens of Truth",
    "LogicLensShadowMQ": "ShT MQ Early Rooms w/o Lens of Truth",
}

# CanDoGlitch's glitch types -> the 3D setting that enables them.
GLITCH_SETTINGS: dict[str, str] = {
    "RestrictedItems": "GlitchRestrictedItems", "SuperStab": "GlitchSuperStab", "ISG": "GlitchISG",
    "BombHover": "GlitchHover", "BombOI": "GlitchBombOI", "OutdoorBombOI": "GlitchBombOI",
    "WindmillBombOI": "GlitchBombOI", "IndoorBombOI": "GlitchBombOI", "DungeonBombOI": "GlitchBombOI",
    "HoverBoost": "GlitchHoverBoost", "SuperSlide": "GlitchSuperSlide", "Megaflip": "GlitchMegaflip",
    "ASlide": "GlitchASlide", "HammerSlide": "GlitchHammerSlide", "LedgeCancel": "GlitchLedgeCancel",
    "ActionSwap": "GlitchActionSwap", "QPA": "GlitchQPA", "HookshotClip": "GlitchHookshotClip",
    "HookshotJump_Bonk": "GlitchHookshotJump_Bonk", "HookshotJump_Boots": "GlitchHookshotJump_Boots",
    "CutsceneDive": "GlitchCutsceneDive", "NaviDive_Stick": "GlitchNaviDive_Stick",
    "TripleSlashClip": "GlitchTripleSlashClip", "LedgeClip": "GlitchLedgeClip", "SeamWalk": "GlitchSeamWalk",
}
DIFFICULTY = {"NOVICE": 1, "INTERMEDIATE": 2, "ADVANCED": 3, "EXPERT": 4, "HERO": 5}

# Settings the client supports at one value (oot3d_options.SUPPORTED), as
# OoT3D_Randomizer reads them in these conditions.
STATIC: dict[str, Any] = {
    "NeedNayrusLove": False,         # no damage multiplier: Link survives a hit
    "FaroresWindAnywhere": False,
    "StartingConsumables": False,
    "MasksAsAdult": False,
    "ShuffleChestMinigame": False,
    "Settings::Enemizer": False,
    "SoulGerudo": True, "SoulDodongo": True, "SoulVolvagia": True, "SoulGohma": True,  # no soul shuffle
    "BONKDAMAGE_ONE": 3,
    "CanBonk": True,                 # bonks do no damage
}


class Unsupported(Exception):
    """A condition uses something without a translation."""


# --- C++ expressions ---------------------------------------------------------

_TOKEN = re.compile(r"\s*(?:(\d+)|([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*)|"
                    r"(&&|\|\||>=|<=|==|!=|[!<>(){},.\[\];+\-?:]))")


def _tokens(text: str) -> list[str]:
    tokens, position = [], 0
    text = text.strip()
    while position < len(text):
        match = _TOKEN.match(text, position)
        if not match or match.end() == position:
            raise Unsupported(f"cannot read {text[position:position + 20]!r}")
        tokens.append(match.group(0).strip())
        position = match.end()
    return tokens


class _Parser:
    """Recursive descent over the C++ subset the conditions use. Nodes are
    tuples: ("num", n), ("name", s), ("call", name, args), ("method", object,
    name, args), ("not", x), ("and", xs), ("or", xs), ("cmp", op, a, b),
    ("lambda", x), ("list", xs)."""

    def __init__(self, text: str) -> None:
        self.tokens = _tokens(text)
        self.index = 0

    def peek(self) -> str | None:
        return self.tokens[self.index] if self.index < len(self.tokens) else None

    def take(self, expected: str | None = None) -> str:
        token = self.peek()
        if token is None or (expected is not None and token != expected):
            raise Unsupported(f"expected {expected!r}, got {token!r}")
        self.index += 1
        return token

    def body(self) -> tuple:
        if self.peek() == "return":
            self.take()
        node = self.expression()
        if self.peek() == ";":
            self.take()
        return node

    def parse(self) -> tuple:
        node = self.body()
        if self.peek() is not None:
            raise Unsupported(f"unexpected {self.peek()!r}")
        return node

    def expression(self) -> tuple:
        items = [self.conjunction()]
        while self.peek() == "||":
            self.take()
            items.append(self.conjunction())
        return items[0] if len(items) == 1 else ("or", items)

    def conjunction(self) -> tuple:
        items = [self.comparison()]
        while self.peek() == "&&":
            self.take()
            items.append(self.comparison())
        return items[0] if len(items) == 1 else ("and", items)

    def comparison(self) -> tuple:
        left = self.unary()
        if self.peek() in (">=", "<=", "==", "!=", ">", "<"):
            operator = self.take()
            return ("cmp", operator, left, self.unary())
        return left

    def unary(self) -> tuple:
        if self.peek() == "!":
            self.take()
            return ("not", self.unary())
        if self.peek() == "-":
            self.take()
            return ("num", -int(self.take()))
        return self.postfix()

    def postfix(self) -> tuple:
        node = self.primary()
        while self.peek() == ".":
            self.take()
            name = self.take()
            if self.peek() == "<":  # Value<u8>()
                self.take()
                self.take()
                self.take(">")
            node = ("method", node, name, self.arguments())
        return node

    def arguments(self) -> list[tuple]:
        self.take("(")
        arguments = []
        while self.peek() != ")":
            arguments.append(self.expression())
            if self.peek() == ",":
                self.take()
        self.take(")")
        return arguments

    def primary(self) -> tuple:
        token = self.peek()
        if token is None:
            raise Unsupported("unexpected end")
        if token.isdigit():
            return ("num", int(self.take()))
        if token == "(":
            self.take()
            node = self.expression()
            self.take(")")
            return node
        if token == "[":  # [] { return ...; }
            self.take()
            self.take("]")
            self.take("{")
            node = self.body()
            self.take("}")
            return ("lambda", node)
        if token == "{":
            self.take()
            items = []
            while self.peek() != "}":
                items.append(self.expression())
                if self.peek() == ",":
                    self.take()
            self.take("}")
            return ("list", items)
        name = self.take()
        if not re.match(r"[A-Za-z_]", name):
            raise Unsupported(f"unexpected {name!r}")
        if self.peek() == "(":
            return ("call", name, self.arguments())
        return ("name", name)


def parse(text: str) -> tuple:
    return _Parser(text).parse()


# --- Rules -------------------------------------------------------------------

Value = Any  # a Python bool or int (known at generation) or a Rule


def _rule(value: Value) -> Rule:
    if isinstance(value, Rule):
        return value
    if isinstance(value, bool):
        return True_() if value else False_()
    raise Unsupported(f"{value!r} is not a condition")


def _all(values: list[Value]) -> Value:
    rules = []
    for value in values:
        if isinstance(value, Rule):
            rules.append(value)
        elif not value:
            return False
    if not rules:
        return True
    result = rules[0]
    for rule in rules[1:]:
        result = result & rule
    return result


def _any(values: list[Value]) -> Value:
    rules = []
    for value in values:
        if isinstance(value, Rule):
            rules.append(value)
        elif value:
            return True
    if not rules:
        return False
    result = rules[0]
    for rule in rules[1:]:
        result = result | rule
    return result


class Context:
    """What a condition is built for: the world (its options) and the region
    the 3D area stands for, whose ages IsChild and IsAdult check."""

    def __init__(self, world: "SohWorld", region: Regions) -> None:
        self.world = world
        self.region = region
        options = world.options
        self.levels = {variable: getattr(options, field).value for variable, field in glitch_options.FIELDS.items()}

    @property
    def bundle(self) -> tuple[Regions, "SohWorld"]:
        return self.region, self.world

    def at(self, region: Regions) -> "Context":
        other = Context.__new__(Context)
        other.world, other.region, other.levels = self.world, region, self.levels
        return other

    # CanDoGlitch, as OoT3D_Randomizer's logic.cpp defines it.
    def enabled(self, setting: str, difficulty: int) -> bool:
        return self.levels[setting] >= difficulty

    def glitch(self, kind: str, difficulty: int) -> Value:
        from .LogicHelpers import (can_jump_slash_except_hammer, can_shield, can_use, has_bottle,
                                   has_bottle_count, has_item, is_adult, is_child)
        bundle = self.bundle
        if kind not in GLITCH_SETTINGS:
            raise Unsupported(f"glitch {kind}")
        if not self.enabled(GLITCH_SETTINGS[kind], difficulty):
            return False
        level = self.levels[GLITCH_SETTINGS[kind]]
        bombs = can_use(Items.BOMB_BAG, bundle)
        bombchus = can_use(Items.BOMBCHU_BAG, bundle)
        shield = can_shield(bundle)
        bugs, fish, fairy = (can_use(item, bundle) for item in
                             (Items.BOTTLE_WITH_BUGS, Items.BOTTLE_WITH_FISH, Items.BOTTLE_WITH_FAIRY))
        adult_claim_check = is_adult(bundle) & has_item(Items.CLAIM_CHECK, bundle)
        child_egg = is_child(bundle) & has_item(Items.WEIRD_EGG, bundle)
        boots = has_item(Items.HOVER_BOOTS, bundle) | has_item(Items.IRON_BOOTS, bundle)
        spells = can_use(Items.NAYRUS_LOVE, bundle) | can_use(Items.DINS_FIRE, bundle)
        if kind in ("RestrictedItems", "SuperSlide", "ActionSwap", "CutsceneDive", "SeamWalk"):
            return True
        if kind == "SuperStab":
            return shield & can_use(Items.STICKS, bundle)
        if kind == "ISG":
            return shield & (can_jump_slash_except_hammer(bundle) | can_use(Items.MEGATON_HAMMER, bundle))
        if kind == "BombHover":
            return _all([self.glitch("ISG", 1), _any([bombchus, _all([bombs, self.enabled("GlitchHover", 3)])])])
        if kind == "BombOI":
            return bombs  # and CanSurviveDamage, always true here
        if kind in ("OutdoorBombOI", "WindmillBombOI"):
            hard = max(difficulty, 3)
            anything = _any([has_item(Items.LENS_OF_TRUTH, bundle), has_bottle(bundle), bombchus,
                             has_item(Items.NUTS, bundle), can_use(Items.STICKS, bundle),
                             can_use(Items.FAIRY_SLINGSHOT, bundle),
                             is_child(bundle) & (has_item(Items.MAGIC_BEAN, bundle) | has_item(Items.WEIRD_EGG, bundle)),
                             can_use(Items.BOOMERANG, bundle), can_use(Items.FAIRY_BOW, bundle),
                             can_use(Items.HOOKSHOT, bundle), can_use(Items.HOVER_BOOTS, bundle),
                             can_use(Items.IRON_BOOTS, bundle), can_use(Items.MEGATON_HAMMER, bundle)])
            if kind == "OutdoorBombOI":
                first = can_use(Items.FARORES_WIND, bundle)
                anything = _any([anything, has_item(Items.DINS_FIRE, bundle), has_item(Items.NAYRUS_LOVE, bundle)])
                second = _any([adult_claim_check, bugs, fish, fairy, spells])
            else:
                first = can_use(Items.FARORES_WIND, bundle) | has_item(Items.NAYRUS_LOVE, bundle) \
                    | has_item(Items.DINS_FIRE, bundle)
                second = _any([adult_claim_check, bugs, fish, fairy])
            return _any([_all([first, anything, self.glitch("BombOI", hard), self.glitch("RestrictedItems", 1)]),
                         _all([second, self.glitch("BombOI", difficulty)])])
        if kind == "IndoorBombOI":
            # The last of the 3D alternatives counts boots and the claim check
            # as bottles; here only two bottles count (stricter).
            options = _any([
                adult_claim_check & (has_bottle(bundle) | boots),
                (bugs | fish | fairy) & (has_bottle_count(2) | (is_adult(bundle) & (has_item(Items.CLAIM_CHECK, bundle) | boots)) | child_egg),
                (spells | can_use(Items.FARORES_WIND, bundle)) & has_bottle_count(2)])
            return _all([options, self.glitch("BombOI", difficulty), self.glitch("RestrictedItems", 1)])
        if kind == "DungeonBombOI":
            return _all([_any([adult_claim_check, bugs, fish, fairy, spells, can_use(Items.FARORES_WIND, bundle)]),
                         self.glitch("BombOI", difficulty)])
        if kind == "HoverBoost":
            return bombs & can_use(Items.HOVER_BOOTS, bundle)
        if kind == "Megaflip":
            return _all([shield, _any([bombs, _all([bombchus, level >= difficulty + 2])])])
        if kind == "ASlide":
            return _all([is_child(bundle), shield, _any([bombs, _all([bombchus, level >= difficulty + 2])])])
        if kind == "HammerSlide":
            return can_use(Items.MEGATON_HAMMER, bundle) & can_use(Items.HOVER_BOOTS, bundle) & shield
        if kind == "LedgeCancel":
            return _all([shield, _any([bombs, _all([bombchus, self.enabled("GlitchLedgeCancel", 3)])])])
        if kind == "QPA":
            return _any([_all([bombs, _any([can_use(Items.HOVER_BOOTS, bundle), can_use(Items.IRON_BOOTS, bundle),
                                            self.enabled("GlitchQPA", 2)])]),
                         self.enabled("GlitchQPA", 3)])
        if kind == "HookshotClip":
            return can_use(Items.HOOKSHOT, bundle)
        if kind == "HookshotJump_Bonk":
            return is_adult(bundle) & has_item(Items.HOOKSHOT, bundle)
        if kind == "HookshotJump_Boots":
            return is_adult(bundle) & has_item(Items.HOOKSHOT, bundle) & boots
        if kind == "NaviDive_Stick":
            return is_child(bundle) & has_item(Items.STICKS, bundle)
        if kind == "TripleSlashClip":
            return can_use(Items.KOKIRI_SWORD, bundle) | can_use(Items.MASTER_SWORD, bundle)
        if kind == "LedgeClip":
            return is_adult(bundle)
        raise Unsupported(f"glitch {kind}")

    def trick(self, variable: str) -> Value:
        from .oot3d_tricks import trick_names
        name = TRICKS.get(variable)
        if name is None:
            raise Unsupported(variable)
        if name not in trick_names():
            return False
        options = self.world.options
        return bool(options.enable_all_tricks) or name in options.tricks_in_logic.value

    def area(self, name: str) -> Regions:
        region = region_for(name)
        if region is None:
            raise Unsupported(f"area {name}")
        return region

    # --- names and calls
    def name(self, name: str) -> Value:
        from . import LogicHelpers as h
        bundle = self.bundle
        if name in STATIC:
            return STATIC[name]
        if name in glitch_options.MISC:
            return bool(self.levels[name])
        if name in TRICKS:
            return self.trick(name)
        if name in SONGS:
            return h.has_item(SONGS[name], bundle)
        if name in EVENTS and name not in ("FairyPot", "GossipStoneFairy", "FairyPond", "BugShrub"):
            return h.has_item(EVENTS[name], bundle)
        simple: dict[str, Callable[[], Value]] = {
            "IsChild": lambda: h.is_child(bundle),
            "IsAdult": lambda: h.is_adult(bundle),
            "Bombs": lambda: h.can_use(Items.BOMB_BAG, bundle),
            "HasBombchus": lambda: h.can_use(Items.BOMBCHU_BAG, bundle),
            "HasExplosives": lambda: h.has_explosives(bundle),
            "CanShield": lambda: h.can_shield(bundle),
            "CanJumpslash": lambda: h.can_jump_slash_except_hammer(bundle),
            "Bugs": lambda: h.can_use(Items.BOTTLE_WITH_BUGS, bundle),
            "Fish": lambda: h.can_use(Items.BOTTLE_WITH_FISH, bundle),
            "Fairy": lambda: h.can_use(Items.BOTTLE_WITH_FAIRY, bundle),
            "BlueFire": lambda: h.blue_fire(bundle),
            "HasBottle": lambda: h.has_bottle(bundle),
            "Sticks": lambda: h.has_item(Items.STICKS, bundle),
            "Nuts": lambda: h.has_item(Items.NUTS, bundle),
            "HoverBoots": lambda: h.has_item(Items.HOVER_BOOTS, bundle),
            "IronBoots": lambda: h.has_item(Items.IRON_BOOTS, bundle),
            "HasBoots": lambda: h.has_item(Items.HOVER_BOOTS, bundle) | h.has_item(Items.IRON_BOOTS, bundle),
            "Hookshot": lambda: h.has_item(Items.HOOKSHOT, bundle),
            "Longshot": lambda: h.has_item(Items.LONGSHOT, bundle),
            "Bow": lambda: h.has_item(Items.FAIRY_BOW, bundle),
            "Boomerang": lambda: h.has_item(Items.BOOMERANG, bundle),
            "Slingshot": lambda: h.has_item(Items.FAIRY_SLINGSHOT, bundle),
            "KokiriSword": lambda: h.has_item(Items.KOKIRI_SWORD, bundle),
            "GoronBracelet": lambda: h.has_item(Items.GORONS_BRACELET, bundle),
            "SilverGauntlets": lambda: h.has_item(Items.SILVER_GAUNTLETS, bundle),
            "DinsFire": lambda: h.has_item(Items.DINS_FIRE, bundle),
            "NayrusLove": lambda: h.has_item(Items.NAYRUS_LOVE, bundle),
            "LensOfTruth": lambda: h.has_item(Items.LENS_OF_TRUTH, bundle),
            "ClaimCheck": lambda: h.has_item(Items.CLAIM_CHECK, bundle),
            "WeirdEgg": lambda: h.has_item(Items.WEIRD_EGG, bundle),
            "MagicBean": lambda: h.has_item(Items.MAGIC_BEAN, bundle),
            "MagicBeanPack": lambda: h.has_item(Items.MAGIC_BEAN, bundle),
            "ShardOfAgony": lambda: h.has_item(Items.STONE_OF_AGONY, bundle),
            "GerudoToken": lambda: h.has_item(Items.GERUDO_MEMBERSHIP_CARD, bundle),
            "AdultsWallet": lambda: h.has_item(Items.ADULT_WALLET, bundle),
            "ScarecrowSong": lambda: h.scarecrows_song(bundle),
            "CanSurviveDamage": lambda: True,  # NeedNayrusLove is false
            "CanTakeDamage": lambda: True,     # Fairy || CanSurviveDamage
            "CanTakeDamageTwice": lambda: (h.can_use(Items.BOTTLE_WITH_FAIRY, bundle) & h.has_bottle_count(2))
                                          | h.effective_health_at_least(bundle, 3),
            "CanGetNightTimeGS": lambda: h.can_get_nighttime_gs(bundle),
            "AtDay": lambda: h.at_day(bundle),
            "AtNight": lambda: h.at_night(bundle),
            "CanSummonGossipFairy": lambda: h.call_gossip_fairy(bundle),
            "CanLeaveForest": lambda: h.can_leave_forest(bundle),
            "HookshotOrBoomerang": lambda: h.hookshot_or_boomerang(bundle),
            "CanUseMagicArrow": lambda: h.can_use_any([Items.FIRE_ARROW, Items.ICE_ARROW, Items.LIGHT_ARROW], bundle),
            "HasFireSource": lambda: h.has_fire_source(bundle),
            "HasFireSourceWithTorch": lambda: h.has_fire_source_with_torch(bundle),
            "CanAdultAttack": lambda: h.is_adult(bundle) & h.can_attack(bundle),
            "CanChildAttack": lambda: h.is_child(bundle) & h.can_attack(bundle),
            "CanUseProjectile": lambda: h.has_projectile(bundle),
            "CanOpenBombGrotto": lambda: h.can_open_bomb_grotto(bundle),
            "BossKeyFireTemple": lambda: h.has_item(Items.FIRE_TEMPLE_BOSS_KEY, bundle),
            "IsKeysanity": lambda: self.world.options.small_key_shuffle.current_key not in ("vanilla", "own_dungeon"),
        }
        if name in simple:
            return simple[name]()
        raise Unsupported(name)

    def call(self, name: str, arguments: list[tuple]) -> Value:
        from . import LogicHelpers as h
        bundle = self.bundle
        if name == "CanDoGlitch":
            kind, difficulty = (self.constant(argument) for argument in arguments)
            return self.glitch(kind.removeprefix("GlitchType::"),
                               DIFFICULTY[difficulty.removeprefix("GlitchDifficulty::")])
        if name == "CanUse":
            item = self.constant(arguments[0])
            if item not in ITEMS:
                raise Unsupported(f"CanUse({item})")
            return h.can_use(ITEMS[item], bundle)
        if name == "CanPlay":
            song = self.constant(arguments[0])
            if song not in SONGS:
                raise Unsupported(f"CanPlay({song})")
            return h.can_use(SONGS[song], bundle)
        if name == "Here":
            region = self.area(self.constant(arguments[0]))
            if arguments[1][0] != "lambda":
                raise Unsupported("Here without a condition")
            inner = self.at(region).evaluate(arguments[1][1])
            return _all([CanReachRegion(str(region)), inner])
        if name in ("AdultCanAccess", "ChildCanAccess"):
            region = self.area(self.constant(arguments[0]))
            return (h.is_adult if name == "AdultCanAccess" else h.is_child)((region, self.world))
        if name == "SmallKeys":
            dungeon = self.constant(arguments[0])
            if dungeon not in KEYS or len(arguments) != 2:
                raise Unsupported(f"SmallKeys({dungeon})")
            return h.small_keys(KEYS[dungeon], self.evaluate(arguments[1]), bundle)
        if name in ("CanSurviveHeatFor", "CanSurviveUnderwaterFor"):
            times = [self.evaluate(argument) for argument in arguments]
            with_ftr, without_ftr = (times + [-1])[:2]
            if without_ftr < 0:
                without_ftr = None
            if with_ftr < 0:
                with_ftr = without_ftr
            tunic = Items.GORON_TUNIC if name == "CanSurviveHeatFor" else Items.ZORA_TUNIC
            alternatives = [h.can_use(tunic, bundle)]
            if with_ftr is not None:
                alternatives.append(h.hearts_at_least(bundle, -(-with_ftr // 8))
                                    & h.can_do_trick(Tricks.FEWER_TUNIC_REQUIREMENTS, bundle))
            if without_ftr is not None:
                alternatives.append(h.hearts_at_least(bundle, -(-without_ftr // 8)))
            return _any(alternatives)
        raise Unsupported(f"{name}()")

    def method(self, subject: tuple, name: str, arguments: list[tuple]) -> Value:
        target = self.constant(subject)
        if name in ("IsVanilla", "IsMQ") and target.startswith("Dungeon::"):
            return name == "IsVanilla"  # the apworld has no Master Quest dungeons
        if name == "Value" and target in glitch_options.MISC:
            return bool(self.levels[target])
        if name == "Value" and target == "BonkDamage":
            return 0
        if name == "Is" and target == "OpenDoorOfTime":
            setting = self.constant(arguments[0])
            if setting != "OPENDOOROFTIME_CLOSED":
                raise Unsupported(setting)
            return self.world.options.door_of_time.current_key == "closed"
        raise Unsupported(f"{target}.{name}")

    @staticmethod
    def constant(node: tuple) -> Any:
        if node[0] == "name":
            return node[1]
        if node[0] == "num":
            return node[1]
        raise Unsupported(f"expected a name, got {node[0]}")

    def compare(self, operator: str, left: tuple, right: tuple) -> Value:
        from . import LogicHelpers as h
        if left[0] == "name" and left[1] in ("NumBottles", "ProgressiveScale") and right[0] == "num":
            count = right[1] + (1 if operator == ">" else 0)
            if operator not in (">=", ">"):
                raise Unsupported(f"{left[1]} {operator}")
            if left[1] == "NumBottles":
                return h.has_bottle_count(count) if count > 0 else True
            scales = [Items.SILVER_SCALE, Items.GOLDEN_SCALE]
            if count > len(scales):
                return False
            return h.has_item(scales[count - 1], self.bundle) if count > 0 else True
        a, b = self.evaluate(left), self.evaluate(right)
        if isinstance(a, Rule) or isinstance(b, Rule) or isinstance(a, bool) or isinstance(b, bool):
            raise Unsupported(f"comparison {operator}")
        return {">=": a >= b, "<=": a <= b, "==": a == b, "!=": a != b, ">": a > b, "<": a < b}[operator]

    def evaluate(self, node: tuple) -> Value:
        kind = node[0]
        if kind == "num":
            return node[1]
        if kind == "name":
            return self.name(node[1])
        if kind == "and":
            return _all([self.evaluate(item) for item in node[1]])
        if kind == "or":
            return _any([self.evaluate(item) for item in node[1]])
        if kind == "not":
            value = self.evaluate(node[1])
            if isinstance(value, Rule):
                raise Unsupported("negated condition")  # could only make the logic expect more
            return not value
        if kind == "cmp":
            return self.compare(node[1], node[2], node[3])
        if kind == "call":
            return self.call(node[1], node[2])
        if kind == "method":
            return self.method(node[1], node[2], node[3])
        raise Unsupported(kind)


# --- Applying them -----------------------------------------------------------

def _load() -> dict[str, Any]:
    data = pkgutil.get_data(__name__, "data/glitched_logic.json")
    if data is None:
        raise FileNotFoundError("apworld data/glitched_logic.json is missing")
    return json.loads(data.decode("utf-8"))


_DATA = _load()
RULES: list[dict[str, str]] = _DATA["rules"]
# Glitchless conditions that need more keys with glitched logic.
KEY_COUNTS: list[dict[str, Any]] = _DATA["keys"]

# OoT3D_Randomizer's SmallKeys: when the glitched key counts apply.
KEY_GLITCHES: dict[str, Callable[[dict[str, int]], bool]] = {
    "FOREST_TEMPLE": lambda levels: levels["GlitchHookshotJump_Boots"] >= 2 or levels["GlitchHoverBoost"] >= 1
                                    or (levels["GlitchHover"] >= 1 and levels["GlitchISG"] >= 2),
    "FIRE_TEMPLE": lambda levels: levels["GlitchLedgeClip"] >= 2 or levels["GlitchHover"] >= 2,
    "SHADOW_TEMPLE": lambda levels: levels["GlitchHookshotClip"] >= 1,
}


def region_for(area: str) -> Regions | None:
    return Regions.__members__.get(AREA_ALIASES.get(area, area))


def _location_names() -> dict[str, str]:
    from .oot3d_data import LOCATIONS
    return {location["enum"]: location["name"] for location in LOCATIONS if not location["master_quest"]}


class GlitchedLogic:
    """The glitched conditions for one world, waiting for the entrances,
    locations and events they extend (LogicHelpers asks while it builds
    the regions)."""

    def __init__(self, world: "SohWorld") -> None:
        self.world = world
        self.entrances: dict[tuple[str, str], list[tuple[dict, Regions, tuple]]] = {}
        self.locations: dict[str, list[tuple[dict, Regions, tuple]]] = {}
        self.events: dict[tuple[str, str], list[tuple[dict, Regions, tuple]]] = {}
        self.skipped: list[tuple[dict[str, str], str]] = []
        # (parent, child) or a location name -> (key, count) its SoH rule also needs.
        self.keys: dict[tuple[str, str] | str, tuple[Items, int]] = {}
        names = _location_names()
        levels = Context(world, Regions.ROOT).levels
        for entry in KEY_COUNTS:
            if not KEY_GLITCHES[entry["dungeon"]](levels):
                continue
            region, target = region_for(entry["area"]), region_for(entry["target"])
            if region is None or (entry["kind"] == "Entrance" and target is None):
                self.skipped.append((entry, "key count without a SoH region"))
                continue
            spot = (str(region), str(target)) if entry["kind"] == "Entrance" else names[entry["target"]]
            self.keys[spot] = (KEYS[entry["dungeon"]], entry["count"])
        for entry in RULES:
            try:
                region = region_for(entry["area"])
                if region is None:
                    raise Unsupported(f"area {entry['area']}")
                node = parse(entry["rule"])
                if entry["kind"] == "Entrance":
                    target = region_for(entry["target"])
                    if target is None:
                        raise Unsupported(f"area {entry['target']}")
                    self.entrances.setdefault((str(region), str(target)), []).append((entry, region, node))
                elif entry["kind"] == "LocationAccess":
                    name = names.get(entry["target"])
                    if name is None:
                        raise Unsupported(f"location {entry['target']}")
                    self.locations.setdefault(name, []).append((entry, region, node))
                else:
                    event = EVENTS.get(entry["target"])
                    if event is None:
                        raise Unsupported(f"event {entry['target']}")
                    self.events.setdefault((str(region), str(event)), []).append((entry, region, node))
            except Unsupported as reason:
                self.skipped.append((entry, str(reason)))

    def build(self, entry: dict[str, str], region: Regions, node: tuple) -> Rule | None:
        try:
            return _rule(Context(self.world, region).evaluate(node))
        except Unsupported as reason:
            self.skipped.append((entry, str(reason)))
            return None

    def _glitched(self, items: list[tuple[dict, Regions, tuple]], parent: str | None) -> Rule | None:
        rules = []
        for entry, region, node in items:
            rule = self.build(entry, region, node)
            if rule is None:
                continue
            if parent is not None and str(region) != parent:
                rule = CanReachRegion(str(region)) & rule
            rules.append(rule)
        return _rule(_any(rules)) if rules else None

    def _keys(self, spot: tuple[str, str] | str, rule: Rule) -> Rule:
        if spot not in self.keys:
            return rule
        from .LogicHelpers import small_keys
        key, count = self.keys.pop(spot)
        return rule & small_keys(key, count, (Regions.ROOT, self.world))

    def extend_entrance(self, parent: str, child: str, rule: Rule) -> Rule:
        rule = self._keys((parent, child), rule)
        items = self.entrances.pop((parent, child), None)
        glitched = self._glitched(items, None) if items else None
        return rule if glitched is None else rule | glitched

    def extend_location(self, parent: str, name: str, rule: Rule) -> Rule:
        rule = self._keys(name, rule)
        items = self.locations.pop(name, None)
        glitched = self._glitched(items, parent) if items else None
        return rule if glitched is None else rule | glitched

    def extend_event(self, parent: str, event: str, rule: Rule) -> Rule:
        items = self.events.pop((parent, event), None)
        glitched = self._glitched(items, parent) if items else None
        return rule if glitched is None else rule | glitched

    def finish(self) -> None:
        """Entrances SoH does not have; what found nothing to extend."""
        for (parent, child), items in list(self.entrances.items()):
            glitched = self._glitched(items, None)
            if glitched is not None:
                self.world.create_entrance(self.world.get_region(parent), self.world.get_region(child), glitched)
        self.entrances.clear()
        # An event SoH sets elsewhere (or not at all) keeps its SoH rules.
        for items in self.events.values():
            self.skipped.extend((entry, "no such SoH event in this region") for entry, _, _ in items)
        self.events.clear()
        # Locations still waiting are not in this seed (shuffles that are off).
        self.locations.clear()
        # A key count SoH has no matching entrance for: the 3D condition
        # sits between regions SoH splits differently.
        self.skipped.extend(({"area": str(spot), "kind": "keys", "target": str(key)}, "key count not applied")
                            for spot, key in self.keys.items())
        self.keys.clear()

def enabled(world: "SohWorld") -> bool:
    return bool(world.options.logic_rules.value) and not world.options.true_no_logic
