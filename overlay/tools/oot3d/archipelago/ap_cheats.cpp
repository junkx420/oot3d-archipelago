#include "ap_cheats.h"

#include "ap_guest_layout.h"

#include <algorithm>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

// An ammo item: its inventory slot, its row in gUpgradeCapacities and the
// shift of its level in SaveContext.upgrades.
struct AmmoItem {
  std::uint8_t Slot;
  std::uint8_t Row;
  std::uint8_t Shift;
};
constexpr std::array<AmmoItem, 5> kAmmoItems{{
    {0x00U, 6U, 17U}, // Deku Sticks
    {0x01U, 7U, 20U}, // Deku Nuts
    {0x02U, 1U, 3U},  // Bombs
    {0x03U, 0U, 0U},  // Bow
    {0x06U, 5U, 14U}, // Slingshot
}};
constexpr std::uint32_t kWalletRow = 4U;
constexpr std::uint32_t kWalletShift = 12U;

bool Raise8(MemoryBus &memory, std::uint32_t offset, std::int8_t target) {
  std::uint8_t value = 0;
  if (!memory.Read8(kSaveContext + offset, &value) || static_cast<std::int8_t>(value) >= target)
    return false;
  return memory.Write8(kSaveContext + offset, static_cast<std::uint8_t>(target));
}

bool Raise16(MemoryBus &memory, std::uint32_t offset, std::int16_t target) {
  std::uint16_t value = 0;
  if (!memory.Read16(kSaveContext + offset, &value) || static_cast<std::int16_t>(value) >= target)
    return false;
  return memory.Write16(kSaveContext + offset, static_cast<std::uint16_t>(target));
}

} // namespace

bool CheatApplier::VerifyCapacities(MemoryBus &memory) {
  if (mVerified || !mDisabled.empty())
    return mVerified;
  for (std::size_t i = 0; i < kUpgradeCapacitiesExpected.size(); ++i) {
    std::uint16_t value = 0;
    if (!memory.Read16(kUpgradeCapacities + static_cast<std::uint32_t>(i) * 2U, &value))
      return false; // not mapped yet
    if (value != kUpgradeCapacitiesExpected[i]) {
      mDisabled = "gUpgradeCapacities does not match the supported EUR image";
      return false;
    }
  }
  mVerified = true;
  return true;
}

int CheatApplier::Apply(const CheatOptions &options, MemoryBus &memory) {
  int written = 0;
  if (options.InfiniteHealth) {
    std::uint16_t capacity = 0, health = 0;
    if (memory.Read16(kSaveContext + kSaveHealthCapacity, &capacity) &&
        memory.Read16(kSaveContext + kSaveHealth, &health) && static_cast<std::int16_t>(health) > 0)
      written += Raise16(memory, kSaveHealth, static_cast<std::int16_t>(capacity)) ? 1 : 0;
  }
  if (options.InfiniteMagic) {
    // The meter as the game gives it: 0x30 per magic level.
    std::uint8_t acquired = 0, level = 0;
    if (memory.Read8(kSaveContext + kSaveMagicAcquired, &acquired) &&
        memory.Read8(kSaveContext + kSaveMagicLevel, &level) && acquired != 0U && (level == 1U || level == 2U))
      written += Raise8(memory, kSaveMagic, static_cast<std::int8_t>(level * 0x30U)) ? 1 : 0;
  }
  if (!(options.InfiniteAmmo || options.InfiniteRupees) || !VerifyCapacities(memory))
    return written;
  std::uint32_t upgrades = 0;
  if (!memory.Read32(kSaveContext + kSaveUpgrades, &upgrades))
    return written;
  const auto capacity = [&](std::uint32_t row, std::uint32_t level) {
    return kUpgradeCapacitiesExpected[row * 4U + std::min(level, 3U)];
  };
  if (options.InfiniteAmmo) {
    for (const auto &item : kAmmoItems) {
      std::uint8_t id = kItemNone;
      if (!memory.Read8(kSaveContext + kSaveItems + item.Slot, &id) || id == kItemNone)
        continue;
      // Level 0 with the item in hand is the first capacity (sticks and nuts
      // found before an upgrade).
      const std::uint32_t level = std::max((upgrades >> item.Shift) & 7U, 1U);
      written += Raise8(memory, kSaveAmmo + item.Slot, static_cast<std::int8_t>(capacity(item.Row, level))) ? 1 : 0;
    }
    std::uint8_t bombchu = kItemNone;
    if (memory.Read8(kSaveContext + kSaveItems + kSlotBombchu, &bombchu) && bombchu != kItemNone)
      written += Raise8(memory, kSaveAmmo + kSlotBombchu, kBombchuCapacity) ? 1 : 0;
  }
  if (options.InfiniteRupees)
    written += Raise16(memory, kSaveRupees,
                       static_cast<std::int16_t>(capacity(kWalletRow, (upgrades >> kWalletShift) & 3U)))
                   ? 1
                   : 0;
  return written;
}

} // namespace Oot3dArchipelago
