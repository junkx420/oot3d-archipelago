#pragma once

// The Great Fairies of the upgrade fountain (scene 0x3B: Death Mountain
// Trail, Death Mountain Crater, outside Ganon's Castle) write their reward
// into the save themselves, with no Item_Give: the magic meter, double magic,
// double defense. OoT3D_Randomizer replaces the reward
// (Cutscene_OverrideFairyReward); the client instead notices the save change,
// reports the delayed location (DMT 7, DMC 8, OGC 9) and takes the game's
// own reward back when the location holds something else. The spell
// fountain's rewards (Din's Fire, Farore's Wind, Nayru's Love) go through
// Item_Give and are direct gives (ap_location_key.h).

#include "recomp/a32_runtime.h"

#include <cstdint>
#include <optional>

namespace Oot3dArchipelago {

inline constexpr std::uint8_t kSceneGreatFairyUpgrades = 0x3BU;

struct FairyUpgrades {
  bool Magic = false;
  bool DoubleMagic = false;
  bool DoubleDefense = false;
  bool operator==(const FairyUpgrades &) const = default;
};

std::optional<FairyUpgrades> ReadFairyUpgrades(oot3d::recomp::a32::MemoryBus &memory);

// The delayed location a change from `before` to `after` stands for, if it
// is a Great Fairy's: double magic before magic (a fairy may set both),
// then double defense.
std::optional<std::uint8_t> FairyRewardLocation(const FairyUpgrades &before, const FairyUpgrades &after);

// Undoes the reward of that location, back to `before`.
bool RevertFairyReward(std::uint8_t delayedFlag, const FairyUpgrades &before,
                       oot3d::recomp::a32::MemoryBus &memory);

} // namespace Oot3dArchipelago
