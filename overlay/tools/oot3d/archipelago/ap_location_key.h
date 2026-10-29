#pragma once

// Archipelago location keys for OoT3D.
//
// The key is OoT3D_Randomizer's ItemOverride_Key (shared/s_item_override.h):
// (scene, type, flag), built by ItemOverride_GetSearchKey
// (code/src/item_override.c). Using the same key keeps the AP location table
// generable from the randomizer's location data.

#include <cstdint>
#include <span>
#include <vector>
#include <optional>
#include <string>
#include <string_view>

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
inline constexpr std::uint16_t kActorCow = 0x01C6U;
inline constexpr std::uint16_t kActorShopkeeper = 0x003DU;
// A cow's location: base item 0x14 (GI_MILK_BOTTLE) + its number, as
// OoT3D_Randomizer's EnCow_ItemOverride gives it; the grottos tell their cow
// apart by the scene they return to.
inline constexpr std::uint8_t kGetItemMilkBottle = 0x14U;
inline constexpr std::uint8_t kSceneDeathMountainTrail = 0x60U;
inline constexpr std::uint8_t kSceneHyruleField = 0x51U;
inline constexpr std::uint8_t kSceneTreasureBoxShop = 0x10U;
inline constexpr std::uint8_t kSceneGrottos = 0x3EU;
inline constexpr std::uint8_t kGetItemRupeePurpleLose = 0x75U;
inline constexpr std::uint8_t kItem00RecoveryHeart = 0x03U;
inline constexpr std::uint8_t kItem00HeartPiece = 0x06U;
inline constexpr std::uint8_t kItem00SmallKey = 0x11U;
inline constexpr std::uint8_t kItemSkullToken = 0x71U;
inline constexpr std::uint8_t kItemFairyOcarina = 0x07U;
inline constexpr std::uint16_t kActorChildZelda = 0x01D3U;
inline constexpr std::uint8_t kGetItemZeldasLetter = 0x0BU;
inline constexpr std::uint8_t kItemDinsFire = 0x05U;
inline constexpr std::uint8_t kItemFaroresWind = 0x0DU;
inline constexpr std::uint8_t kItemLightArrows = 0x12U;
inline constexpr std::uint8_t kItemNayrusLove = 0x13U;
inline constexpr std::uint8_t kItemRecoveryHeart = 0x83U;
// Delayed locations (flags) besides the songs (OoT3D_Randomizer
// s_item_override.h, DelayedOverride).
inline constexpr std::uint8_t kDelayedLightArrowCutscene = 1U;
inline constexpr std::uint8_t kDelayedSariasGift = 2U;
inline constexpr std::uint8_t kDelayedZeldasLetter = 3U;
inline constexpr std::uint8_t kDelayedFairyZorasFountain = 4U;
inline constexpr std::uint8_t kDelayedFairyHyruleCastle = 5U;
inline constexpr std::uint8_t kDelayedFairyColossus = 6U;
inline constexpr std::uint8_t kDelayedFairyMountainTrail = 7U;
inline constexpr std::uint8_t kDelayedFairyCrater = 8U;
inline constexpr std::uint8_t kDelayedFairyOutsideGanonsCastle = 9U;
inline constexpr std::uint8_t kItemMedallionForest = 0x66U; // then Fire, Water, Spirit, Shadow
inline constexpr std::uint8_t kItemMedallionLight = 0x6BU;
inline constexpr std::uint8_t kItemKokiriEmerald = 0x6CU;   // then Goron Ruby
inline constexpr std::uint8_t kItemZoraSapphire = 0x6EU;
// Dungeon reward locations (type temple): flag = dungeon index, Link's Pocket
// apart (OoT3D_Randomizer item_override.c).
inline constexpr std::uint8_t kTempleScene = 0xFFU;
inline constexpr std::uint8_t kLinksPocketFlag = 0xFFU;
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

// The skull keys of the tokens a save's gsFlags record (array index and bit,
// as KeyForOffer forms them from a token's params).
std::vector<LocationKey> TokenKeys(std::span<const std::uint8_t> gsFlags);

// The song a teacher's location (a delayed song key) gives in the game, as
// the item table names it; nothing for other keys.
std::optional<std::string_view> TeacherSong(const LocationKey &key);
// The reward a dungeon reward's location (a temple key) gives in the game.
std::optional<std::string_view> DungeonReward(const LocationKey &key);

std::string Describe(const LocationKey &key);

} // namespace Oot3dArchipelago
