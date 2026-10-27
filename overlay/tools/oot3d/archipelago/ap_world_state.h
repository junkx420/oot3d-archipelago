#pragma once

// World settings from the seed (slot_data) that the unmodified game expresses
// in save flags, set as OoT3D_Randomizer's SaveFile_Init sets them for a new
// save. Only settings that need nothing else from the game are here: Jabu-Jabu
// open, King Zora moved, the rainbow bridge built, Ganon's trials that the
// seed does not require, the Door of Time open, the Kakariko gate open (its
// guard has heard about the mask shop, so the shop opens early too) and Mido
// moved aside. The forest exit is a check in the game's code instead
// (ap_check_observer.h, Kokiri guard).

#include "recomp/a32_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Oot3dArchipelago {

struct WorldState {
  bool OpenJabuJabu = false;
  bool OpenZorasFountain = false;
  bool OpenRainbowBridge = false;
  bool OpenDoorOfTime = false;
  bool OpenKakarikoGate = false;
  bool MidoMoved = false;       // closed_forest off
  bool OpenForestExit = false;  // closed_forest deku_only or off
  // Trials already done when the file starts, by name ("Forest Trial", ...).
  std::vector<std::string> SkippedTrials;
  [[nodiscard]] bool Empty() const noexcept {
    return !OpenJabuJabu && !OpenZorasFountain && !OpenRainbowBridge && !OpenDoorOfTime && !OpenKakarikoGate &&
           !MidoMoved && SkippedTrials.empty();
  }
};

// From the apworld's slot_data (as JSON text); a missing or malformed field
// leaves its setting as in the game.
WorldState ParseWorldState(const std::string &slotDataJson);

// Sets the flags the loaded save lacks; returns how many it set. Bits are only
// added, so calling it every frame is safe.
int ApplyWorldState(const WorldState &state, oot3d::recomp::a32::MemoryBus &memory);

} // namespace Oot3dArchipelago
