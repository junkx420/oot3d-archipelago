#pragma once

// Damage multiplier (a client option, x1 by default), as OoT3D_Randomizer's
// Settings_ApplyDamageMultiplier (settings.c). The randomizer replaces the
// first instruction of Health_ChangeBy (mov r4, r1); the check observer
// changes r1, the health change, at the function's entry instead. The game
// then applies Master Quest's double damage and double defense itself, as
// it does after the randomizer's hook. Unlike the randomizer, Master Quest
// keeps its double damage: x1 leaves the game as it is, and the others
// multiply what the game deals.
//
// Like the cheats this changes nothing the seed or the server sees; the
// seed's logic assumes normal damage.

#include <cstdint>
#include <optional>
#include <string_view>

namespace Oot3dArchipelago {

// Health_ChangeBy(GlobalContext *, s16 healthChange), EUR.
inline constexpr std::uint32_t kHealthChangeBy = 0x00352DBCU;

enum class DamageMultiplier : std::uint8_t { Half, Normal, Double, Quadruple, Octuple, Sexdecuple, OneHitKo };
const char *ToString(DamageMultiplier multiplier) noexcept;
std::optional<DamageMultiplier> ParseDamageMultiplier(std::string_view text) noexcept;

// What OHKO turns every hit into.
inline constexpr std::int32_t kOneHitKoChange = -1000;

class DamageScaler {
public:
  // The health change Health_ChangeBy is called with, scaled. Healing (and
  // nothing) is left as it is.
  std::int32_t Apply(DamageMultiplier multiplier, std::int32_t change, bool doubleDefense) noexcept;

private:
  // Alternating bits for damage that would round to nothing (as the
  // randomizer's Damage32): 1 for halved quarter hearts, 2 for double defense.
  std::uint8_t mAlternate = 0;
};

} // namespace Oot3dArchipelago
