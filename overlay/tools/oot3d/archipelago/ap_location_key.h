#pragma once

// Archipelago location keys for OoT3D.
//
// The key is OoT3D_Randomizer's ItemOverride_Key (shared/s_item_override.h):
// (scene, type, flag), built by ItemOverride_GetSearchKey
// (code/src/item_override.c). Using the same key keeps the AP location table
// generable from the randomizer's location data.

#include <cstdint>
#include <optional>
#include <string>

namespace Oot3dArchipelago {

enum class OverrideType : std::uint8_t {
  BaseItem = 0,
  Chest = 1,
  Collectable = 2,
  Skull = 3,
  GrottoScrub = 4,
  Delayed = 5,
  Temple = 6,
};

struct LocationKey {
  std::uint8_t Scene = 0;
  OverrideType Type = OverrideType::BaseItem;
  std::uint8_t Flag = 0;

  // ItemOverride_Key.all on the little-endian 3DS: {pad, scene, type, flag}.
  [[nodiscard]] std::uint32_t Packed() const noexcept {
    return (static_cast<std::uint32_t>(Scene) << 8U) |
           (static_cast<std::uint32_t>(Type) << 16U) |
           (static_cast<std::uint32_t>(Flag) << 24U);
  }
  friend bool operator==(const LocationKey &, const LocationKey &) = default;
};

inline constexpr std::uint16_t kActorChest = 0x000AU;
inline constexpr std::uint16_t kActorEnItem00 = 0x0015U;
inline constexpr std::uint16_t kActorDekuBaba = 0x0055U;
inline constexpr std::uint16_t kActorWitheredDekuBaba = 0x00C7U;
inline constexpr std::uint16_t kActorBusinessScrubSalesman = 0x011AU;
inline constexpr std::uint16_t kActorSkullToken = 0x019CU;
inline constexpr std::uint8_t kSceneTreasureBoxShop = 0x10U;
inline constexpr std::uint8_t kSceneGrottos = 0x3EU;
inline constexpr std::uint8_t kGetItemRupeePurpleLose = 0x75U;
inline constexpr std::uint8_t kItem00RecoveryHeart = 0x03U;
inline constexpr std::uint8_t kItem00HeartPiece = 0x06U;
inline constexpr std::uint8_t kItem00SmallKey = 0x11U;
inline constexpr std::uint8_t kItemSkullToken = 0x71U;
inline constexpr std::uint8_t kItemSongMinuet = 0x5AU;
inline constexpr std::uint8_t kItemSongStorms = 0x65U;
// DelayedOverride: DLYOVR_SONG_MINUET is the eleventh entry and the songs
// follow in the same order as their item ids.
inline constexpr std::uint8_t kDelayedSongMinuet = 10U;
inline constexpr std::uint8_t kDelayedScene = 0xFFU;

// What Actor_OfferGetItem sees, read from the guest by the caller.
struct OfferSource {
  std::uint16_t ActorId = 0;
  std::uint16_t ActorParams = 0;
  std::uint8_t Scene = 0;
  // Absolute get-item id (Actor_OfferGetItem accepts negative ids).
  std::uint8_t GetItemId = 0;
  // EnItem00 only: its collectible flag (actor + 0x1A8).
  std::uint16_t CollectibleFlag = 0;
  // Grotto scrubs: gSaveContext.respawn[RESPAWN_MODE_RETURN].data.
  std::uint8_t RespawnReturnScene = 0;
};

// The randomizer's key for an offer, or nothing where it does not override
// (respawning drops, Deku Baba drops, the chest game's losing rupee).
// Vanilla EnItem00 has no randomizer extension, so only collectible flags
// 0x01..0x1F, which the scene layer owns, form keys.
std::optional<LocationKey> KeyForOffer(const OfferSource &source);

// Item_Give without an accepted offer: the songs are the randomizer's
// delayed overrides (scene 0xFF). Other direct gives have no location key.
std::optional<LocationKey> KeyForDirectGive(std::uint8_t item);

std::string Describe(const LocationKey &key);

} // namespace Oot3dArchipelago
