#pragma once

// Save tests the unmodified game makes where OoT3D_Randomizer asks whether a
// location was taken.
//
// Several NPCs give their item only to a Link who does not own it yet:
// Darunia the Goron's Bracelet, the Lake Hylia sun the Fire Arrows, Link the
// Goron his tunic, King Zora his tunic, Biggoron his sword; the child rolling
// Goron talks only to a Link with a bomb bag. With the items shuffled, Link
// may own them from elsewhere (or lack the bomb bag the logic does not ask
// for), and the location could never be taken. OoT3D_Randomizer patches each
// test to ask its own location flag instead (the patches are named below).
//
// The client patches no code: for the one block that makes the test, the
// save shows what the location says (taken: owned; not taken: not owned),
// and the blocks after the test's branch restore it. Link the Goron also
// never sees the Fire Medallion, as in the randomizer.
//
// The minigames that pick their reward by Link's upgrades are keyed by
// KeyForOffer (ap_location_key.h); two of them leave the reward unset for
// some quivers, and the client presets the randomizer's one there.

#include "ap_location_key.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace Oot3dArchipelago {

struct SaveTest {
  const char *Name;
  std::uint32_t CheckBlock;
  std::array<std::uint32_t, 2> RestoreBlocks;
  LocationKey Location;
  std::uint32_t Offset; // in the save
  std::uint8_t Size;    // 1, 2 or 4 bytes
  std::uint32_t Mask;   // the bits the test reads
  // The masked value of "not owned", and the one shown for "owned" when the
  // save holds "not owned".
  std::uint32_t Missing;
  std::uint32_t Owned;
  // Only "not taken" is shown (the bomb bag: Link may keep his own state
  // once the location is taken). Shown as owned, not as missing.
  bool OwnedUntilTaken = false;
};

// The tests' scenes (Death Mountain Trail: ap_location_key.h).
inline constexpr std::uint8_t kSceneGoronCity = 0x62U;
inline constexpr std::uint8_t kSceneLakeHylia = 0x57U;
inline constexpr std::uint8_t kSceneZorasDomain = 0x58U;

// The song teachers' locations (delayed, OoT3D_Randomizer's DelayedOverride
// order) and their songs' questItems bits.
inline constexpr std::uint8_t kDelayedSongLullaby = kDelayedSongMinuet + 6U;
inline constexpr std::uint8_t kDelayedSongEpona = kDelayedSongMinuet + 7U;
inline constexpr std::uint8_t kDelayedSongSaria = kDelayedSongMinuet + 8U;
inline constexpr std::uint8_t kDelayedSongSun = kDelayedSongMinuet + 9U;
inline constexpr std::uint32_t kQuestSongLullaby = 1U << 12U;
inline constexpr std::uint32_t kQuestSongEpona = 1U << 13U;
inline constexpr std::uint32_t kQuestSongSaria = 1U << 14U;
inline constexpr std::uint32_t kQuestSongSun = 1U << 15U;

inline constexpr std::array<SaveTest, 12> kSaveTests{{
    // DaruniaCheckStrength: upgrades strength (mask 0x1C0, shift 6) below 1.
    {"Darunia's strength", 0x001E4878U, {0x001E48B0U, 0x001E48BCU},
     {kSceneGoronCity, OverrideType::BaseItem, 0x54U}, 0xB8U, 4U, 0x1C0U, 0U, 0x40U},
    // FireArrowCheckChestFlagOne and Two: items[SLOT_ARROW_FIRE] empty.
    {"the sun's Fire Arrows", 0x003F23D0U, {0x003F2404U, 0x003F2424U},
     {kSceneLakeHylia, OverrideType::BaseItem, 0x58U}, 0x8CU + 4U, 1U, 0xFFU, 0xFFU, 0x04U},
    {"the sun's Fire Arrows", 0x003F2440U, {0x003F2450U, 0x003F24CCU},
     {kSceneLakeHylia, OverrideType::BaseItem, 0x58U}, 0x8CU + 4U, 1U, 0xFFU, 0xFFU, 0x04U},
    // AdultRollingGoronOne: equipment Goron Tunic (2 << 8).
    {"Link the Goron's tunic", 0x0019546CU, {0x00195484U, 0x0019549CU},
     {kSceneGoronCity, OverrideType::BaseItem, 0x2CU}, 0xB6U, 2U, 0x200U, 0U, 0x200U},
    // KingZoraGiveTunic: equipment Zora Tunic (4 << 8).
    {"King Zora's tunic", 0x001B6A54U, {0x001B6844U, 0U},
     {kSceneZorasDomain, OverrideType::BaseItem, 0x2DU}, 0xB6U, 2U, 0x400U, 0U, 0x400U},
    // BiggoronCheckBGSFlag: SaveContext.bgsFlag.
    {"Biggoron's sword", 0x00194D90U, {0x00194DB0U, 0x00195310U},
     {kSceneDeathMountainTrail, OverrideType::BaseItem, 0x57U}, 0x52U, 1U, 0xFFU, 0U, 1U},
    // ChildRollingGoronDontCheckBombBag: upgrades bomb bag (mask 0x38,
    // shift 3) of at least 20 bombs.
    {"the rolling Goron's bomb bag", 0x001953E4U, {0x00195418U, 0x00195434U},
     {kSceneGoronCity, OverrideType::BaseItem, 0x34U}, 0xB8U, 4U, 0x38U, 0U, 0x08U, true},
    // Song teachers teach only a Link who does not know their song; with
    // songs shuffled he may know it from elsewhere. LullabyCheckFlag (Impa),
    // EponasSongCheckFlag and Two (Malon), SariasSongCheckFlag (Saria),
    // SunsSongLocation (the Royal Family's Tomb). questItems at 0xBC.
    {"Impa's Zelda's Lullaby", 0x0044FD6CU, {0x0044FD7CU, 0x0044FDB4U},
     {kDelayedScene, OverrideType::Delayed, kDelayedSongLullaby}, 0xBCU, 4U, kQuestSongLullaby, 0U, kQuestSongLullaby},
    {"Malon's Epona's Song", 0x0018C560U, {0x0018C574U, 0x0018C594U},
     {kDelayedScene, OverrideType::Delayed, kDelayedSongEpona}, 0xBCU, 4U, kQuestSongEpona, 0U, kQuestSongEpona},
    {"Malon's Epona's Song", 0x00195CECU, {0x00195D04U, 0x00195D0CU},
     {kDelayedScene, OverrideType::Delayed, kDelayedSongEpona}, 0xBCU, 4U, kQuestSongEpona, 0U, kQuestSongEpona},
    {"Saria's Song", 0x00168698U, {0x001686ACU, 0x001686F0U},
     {kDelayedScene, OverrideType::Delayed, kDelayedSongSaria}, 0xBCU, 4U, kQuestSongSaria, 0U, kQuestSongSaria},
    {"the tomb's Sun's Song", 0x002888B4U, {0x002888CCU, 0x002888F0U},
     {kDelayedScene, OverrideType::Delayed, kDelayedSongSun}, 0xBCU, 4U, kQuestSongSun, 0U, kQuestSongSun},
}};

// AdultRollingGoronIgnoreMedallion: the dispatch block of the Gorons' texts
// (params & 0x1F selects one; Link the Goron is 1), and the blocks after his
// Fire Medallion test (questItems bit 1).
inline constexpr std::uint32_t kGoronTextDispatchBlock = 0x00195380U;
inline constexpr std::uint32_t kGoronLinkMedallionBlock = 0x00195454U;
inline constexpr std::uint32_t kGoronLinkParams = 1U;
inline constexpr std::uint32_t kQuestFireMedallion = 1U << 1U;

// AdultShootingGallery and GerudoArcheryOne/Two: the quiver branch of the
// adult shooting gallery (reward at actor + 0x584) and of horseback archery
// (reward in r4, r5). The game sets nothing for the quivers it does not
// expect; the client presets 40 arrows (KeyForOffer folds 50 into it).
inline constexpr std::uint32_t kAdultGalleryQuiverBlock = 0x0038AB88U;
inline constexpr std::uint32_t kAdultGalleryReward = 0x584U;
inline constexpr std::uint32_t kArcheryQuiverBlock = 0x003AFA10U;    // r4
inline constexpr std::uint32_t kArcheryQuiverTwoBlock = 0x003C2D5CU; // r5
inline constexpr std::uint8_t kGetItemQuiver40 = 0x30U;

// Every block of the save tests, for the hook list.
std::span<const std::uint32_t> SaveTestHookPcs() noexcept;

// The value a test's save field shows: the location taken shows "owned",
// otherwise "not owned" (OwnedUntilTaken: "owned" until taken, then the
// save's own). Bits outside the mask are kept.
std::uint32_t SaveTestValue(const SaveTest &test, std::uint32_t value, bool taken) noexcept;

} // namespace Oot3dArchipelago
