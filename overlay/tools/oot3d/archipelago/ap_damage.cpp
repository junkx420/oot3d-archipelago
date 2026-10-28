#include "ap_damage.h"

#include <array>

namespace Oot3dArchipelago {
namespace {

constexpr std::array<DamageMultiplier, 7> kMultipliers{
    DamageMultiplier::Half,    DamageMultiplier::Normal,     DamageMultiplier::Double,  DamageMultiplier::Quadruple,
    DamageMultiplier::Octuple, DamageMultiplier::Sexdecuple, DamageMultiplier::OneHitKo};

} // namespace

const char *ToString(DamageMultiplier multiplier) noexcept {
  switch (multiplier) {
  case DamageMultiplier::Half:
    return "half";
  case DamageMultiplier::Normal:
    return "normal";
  case DamageMultiplier::Double:
    return "x2";
  case DamageMultiplier::Quadruple:
    return "x4";
  case DamageMultiplier::Octuple:
    return "x8";
  case DamageMultiplier::Sexdecuple:
    return "x16";
  case DamageMultiplier::OneHitKo:
    return "ohko";
  }
  return "normal";
}

std::optional<DamageMultiplier> ParseDamageMultiplier(std::string_view text) noexcept {
  for (const auto multiplier : kMultipliers)
    if (text == ToString(multiplier))
      return multiplier;
  return std::nullopt;
}

std::int32_t DamageScaler::Apply(DamageMultiplier multiplier, std::int32_t change, bool doubleDefense) noexcept {
  if (change >= 0 || multiplier == DamageMultiplier::Normal)
    return change;
  if (multiplier == DamageMultiplier::OneHitKo)
    return kOneHitKoChange;
  std::int32_t scaled = change;
  switch (multiplier) {
  case DamageMultiplier::Half:
    scaled /= 2;
    break;
  case DamageMultiplier::Double:
    scaled *= 2;
    break;
  case DamageMultiplier::Quadruple:
    scaled *= 4;
    break;
  case DamageMultiplier::Octuple:
    scaled *= 8;
    break;
  case DamageMultiplier::Sexdecuple:
    scaled *= 16;
    break;
  default:
    break;
  }
  // Half of a quarter heart: alternate -1 and 0.
  if (scaled == 0) {
    scaled = -static_cast<std::int32_t>(mAlternate & 1U);
    mAlternate ^= 1U;
  }
  // The game halves damage for double defense with an arithmetic shift, which
  // leaves -1 whole: alternate -2 and 0 instead.
  if (doubleDefense && scaled == -1) {
    scaled = -static_cast<std::int32_t>(mAlternate & 2U);
    mAlternate ^= 2U;
  }
  return scaled;
}

} // namespace Oot3dArchipelago
