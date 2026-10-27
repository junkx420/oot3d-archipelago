#include "ap_fairy_rewards.h"

#include "ap_guest_layout.h"
#include "ap_location_key.h"

#include <algorithm>

namespace Oot3dArchipelago {

using oot3d::recomp::a32::MemoryBus;

std::optional<FairyUpgrades> ReadFairyUpgrades(MemoryBus &memory) {
  std::uint8_t magic = 0;
  std::uint8_t doubleMagic = 0;
  std::uint8_t doubleDefense = 0;
  if (!memory.Read8(kSaveContext + kSaveMagicAcquired, &magic) ||
      !memory.Read8(kSaveContext + kSaveDoubleMagic, &doubleMagic) ||
      !memory.Read8(kSaveContext + kSaveDoubleDefense, &doubleDefense))
    return std::nullopt;
  return FairyUpgrades{magic != 0U, doubleMagic != 0U, doubleDefense != 0U};
}

std::optional<std::uint8_t> FairyRewardLocation(const FairyUpgrades &before, const FairyUpgrades &after) {
  if (after.DoubleMagic && !before.DoubleMagic)
    return kDelayedFairyCrater;
  if (after.Magic && !before.Magic)
    return kDelayedFairyMountainTrail;
  if (after.DoubleDefense && !before.DoubleDefense)
    return kDelayedFairyOutsideGanonsCastle;
  return std::nullopt;
}

// The meter as the item table gives it (ap_item_table.cpp GiveMagic,
// GiveDoubleMagic): level, acquired, meter size and a full meter.
bool RevertFairyReward(std::uint8_t delayedFlag, const FairyUpgrades &before, MemoryBus &memory) {
  const auto write8 = [&](std::uint32_t offset, std::uint8_t value) {
    return memory.Write8(kSaveContext + offset, value);
  };
  if (delayedFlag == kDelayedFairyOutsideGanonsCastle)
    return write8(kSaveDoubleDefense, before.DoubleDefense ? 1U : 0U);
  if (delayedFlag != kDelayedFairyMountainTrail && delayedFlag != kDelayedFairyCrater)
    return false;
  const std::uint8_t level = before.DoubleMagic ? 2U : before.Magic ? 1U : 0U;
  const std::uint8_t size = static_cast<std::uint8_t>(level * 0x30U);
  std::uint8_t magic = 0;
  (void)memory.Read8(kSaveContext + kSaveMagic, &magic);
  return write8(kSaveMagicAcquired, before.Magic ? 1U : 0U) &&
         write8(kSaveDoubleMagic, before.DoubleMagic ? 1U : 0U) && write8(kSaveMagicLevel, level) &&
         memory.Write16(kSaveContext + kSaveMagicMeterSize, size) &&
         write8(kSaveMagic, std::min(magic, size));
}

} // namespace Oot3dArchipelago
