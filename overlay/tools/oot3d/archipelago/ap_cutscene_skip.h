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

#include "recomp/a32_runtime.h"

#include <cstdint>

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

} // namespace Oot3dArchipelago
