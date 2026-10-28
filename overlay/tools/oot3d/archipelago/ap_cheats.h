#pragma once

// Optional cheats (client settings, all off by default), after Ship of
// Harkinian's: infinite health, magic, ammo and rupees.
//
// Each one tops up a value in the loaded save once per frame, never above
// what the save allows: the heart capacity, the magic meter the save has, and
// the ammo and rupee capacity of its upgrades from the game's own table
// (gUpgradeCapacities). Nothing is given that the save does not have: no
// magic without the meter, no ammo for an item not yet found. They change
// nothing the logic or the server sees.

#include "recomp/a32_runtime.h"

#include <array>
#include <cstdint>
#include <string>

namespace Oot3dArchipelago {

struct CheatOptions {
  bool InfiniteHealth = false;
  bool InfiniteMagic = false;
  bool InfiniteAmmo = false;
  bool InfiniteRupees = false;

  [[nodiscard]] bool Any() const noexcept { return InfiniteHealth || InfiniteMagic || InfiniteAmmo || InfiniteRupees; }
};

// gUpgradeCapacities, u16[8][4]: quiver, bomb bag, strength, scale, wallet,
// bullet bag, sticks, nuts (as in Ocarina of Time). Found from
// OoT3D_Randomizer's TycoonWalletSize patch site (oot.ld, 0x0053CC08: the
// wallet's third and fourth capacity) and read from the EUR image.
inline constexpr std::uint32_t kUpgradeCapacities = 0x0053CBE4U;
inline constexpr std::array<std::uint16_t, 32> kUpgradeCapacitiesExpected{
    0, 30, 40, 50,  // quiver
    0, 20, 30, 40,  // bomb bag
    0, 0,  0,  0,   // strength
    0, 0,  0,  0,   // scale
    99, 200, 500, 500, // wallet
    0, 30, 40, 50,  // bullet bag
    0, 10, 20, 30,  // sticks
    0, 20, 30, 40}; // nuts
// The bombchu limit (Ocarina of Time's AMMO cap for bombchus).
inline constexpr std::int8_t kBombchuCapacity = 50;

class CheatApplier {
public:
  // Tops up what the options ask for; returns how many values it wrote. Call
  // once per frame in gameplay with a save loaded. Health is only raised
  // while Link is alive, so a death (also a DeathLink one) still happens.
  int Apply(const CheatOptions &options, oot3d::recomp::a32::MemoryBus &memory);

  // Why ammo and rupees are off: the capacity table did not match.
  [[nodiscard]] const std::string &Disabled() const noexcept { return mDisabled; }

private:
  bool VerifyCapacities(oot3d::recomp::a32::MemoryBus &memory);

  bool mVerified = false;
  std::string mDisabled;
};

} // namespace Oot3dArchipelago
