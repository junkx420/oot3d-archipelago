#pragma once

// Optional cutscene skips (client setting, off by default).
//
// The game records which cutscenes and first conversations it has shown in
// save flags. Setting a flag before the scene plays skips it, as
// OoT3D_Randomizer does for every new save (code/src/savefile.c,
// SaveFile_Init). Only flags that skip a cutscene or a conversation are
// set here: none gives an item, opens a path or changes what the logic
// expects. Left out on purpose: the Master Sword and Sheik flags (Rauru's
// medallion is Link's Pocket), Ruto in Jabu-Jabu, Malon, Nabooru as a child,
// the Fire and Water Temple doors, the Dampe race and the minigame phases.

//
// Blue warps: OoT3D_Randomizer replaces a child blue warp's scene change
// (Cutscene_BlueWarpOverride). The game's own code there already skips the
// Deku Tree's speech and death, and the Goron Ruby cutscene, when the
// dungeon's event is set, and warps straight to the dungeon's entrance. The
// client sets that event as the warp starts (ChildBlueWarpBlock, a hook at
// the randomizer's patch site); the dungeon reward is the location's item
// and comes from the server like any other.

#include "recomp/a32_runtime.h"

#include <cstdint>
#include <optional>

namespace Oot3dArchipelago {

struct CutsceneSkipOptions {
  bool Cutscenes = false; // story and boss cutscenes, first conversations
  bool Intro = false;     // the Deku Tree intro of a new file (experimental)
};

// Sets the skip flags the loaded save lacks; returns how many it set. Call
// once per frame while a save is loaded: the game copies a scene's live
// flags back into the save when Link leaves it, so a flag may need setting
// again.
int ApplyCutsceneSkips(const CutsceneSkipOptions &options, oot3d::recomp::a32::MemoryBus &memory);

// Boss rooms of the child dungeons and their dungeon index (the flag of the
// dungeon reward's location, type temple).
inline constexpr std::uint8_t kSceneDekuTreeBoss = 0x11U;
inline constexpr std::uint8_t kSceneDodongosCavernBoss = 0x12U;
inline constexpr std::uint8_t kDungeonDekuTree = 0U;
inline constexpr std::uint8_t kDungeonDodongosCavern = 1U;

// At ChildBlueWarpBlock in a boss room: sets the dungeon's events if the
// save lacks them, so the game skips the cutscene. Returns the dungeon index
// when it did, nothing when the warp plays as usual.
std::optional<std::uint8_t> SkipBlueWarpCutscene(std::uint8_t scene, oot3d::recomp::a32::MemoryBus &memory);

} // namespace Oot3dArchipelago
