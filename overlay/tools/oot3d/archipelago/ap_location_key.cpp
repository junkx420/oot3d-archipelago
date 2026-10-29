#include "ap_location_key.h"

#include <array>
#include <cstdio>

namespace Oot3dArchipelago {
namespace {

// Get-items of the upgrades the unmodified game picks by Link's capacity.
constexpr std::uint8_t kGetItemQuiver40 = 0x30U;
constexpr std::uint8_t kGetItemQuiver50 = 0x31U;
constexpr std::uint8_t kGetItemBombBag30 = 0x33U;
constexpr std::uint8_t kGetItemBombBag40 = 0x34U;
constexpr std::uint8_t kGetItemWalletAdult = 0x45U;
constexpr std::uint8_t kGetItemWalletGiant = 0x46U;
constexpr std::uint8_t kGetItemRupeeGold = 0x56U;
constexpr std::uint8_t kGetItemBulletBag40 = 0x60U;
constexpr std::uint8_t kGetItemStickUpgrade20 = 0x77U;
constexpr std::uint8_t kGetItemStickUpgrade30 = 0x78U;
constexpr std::uint8_t kGetItemNutUpgrade30 = 0x79U;
constexpr std::uint8_t kGetItemNutUpgrade40 = 0x7AU;
constexpr std::uint8_t kGetItemBulletBag50 = 0x7BU;

constexpr std::uint8_t kSceneShootingGallery = 0x42U;
constexpr std::uint8_t kSceneBombchuBowling = 0x4BU;
constexpr std::uint8_t kSceneSkulltulaHouse = 0x50U;
constexpr std::uint8_t kSceneLostWoods = 0x5BU;
constexpr std::uint8_t kSceneGerudoFortress = 0x5DU;
constexpr std::uint8_t kSceneGoronCity = 0x62U;
// The Gold Skulltula family's rewards for 10 and 30 tokens (their params).
constexpr std::uint16_t kSkulltulaReward10 = 1U;
constexpr std::uint16_t kSkulltulaReward30 = 3U;

// The get-item OoT3D_Randomizer makes these offers fixed, and keys their
// locations by (its patches named). The unmodified game picks the next
// capacity, so the same location would be offered under another id.
std::uint8_t RandomizerGetItem(const OfferSource &source) {
  const std::uint8_t item = source.GetItemId;
  if (source.ActorId == kActorBusinessScrubSalesman) {
    // ScrubStickUpgradeOne/Two, ScrubNutUpgradeOne/Two: 20 sticks, 30 nuts.
    if (item == kGetItemStickUpgrade30)
      return kGetItemStickUpgrade20;
    if (item == kGetItemNutUpgrade40)
      return kGetItemNutUpgrade30;
    return item;
  }
  switch (source.Scene) {
  case kSceneGrottos:
    // DekuTheaterSkullMask, DekuTheaterMaskOfTruth: 20 sticks, 40 nuts.
    if (item == kGetItemStickUpgrade30)
      return kGetItemStickUpgrade20;
    if (item == kGetItemNutUpgrade30)
      return kGetItemNutUpgrade40;
    break;
  case kSceneShootingGallery:
    // ChildShootingGallery, AdultShootingGallery.
    if (item == kGetItemBulletBag50)
      return kGetItemBulletBag40;
    if (item == kGetItemQuiver50)
      return kGetItemQuiver40;
    break;
  case kSceneLostWoods:
    // LostWoodsShootingGame (One and Two).
    if (item == kGetItemBulletBag50)
      return kGetItemBulletBag40;
    break;
  case kSceneGerudoFortress:
    // GerudoArcheryOne/Two.
    if (item == kGetItemQuiver50)
      return kGetItemQuiver40;
    break;
  case kSceneBombchuBowling:
    // BombchuBowlingStaticReward.
    if (item == kGetItemBombBag40)
      return kGetItemBombBag30;
    break;
  case kSceneGoronCity:
    // ChildRollingGoron.
    if (item == kGetItemBombBag30)
      return kGetItemBombBag40;
    break;
  case kSceneSkulltulaHouse:
    // SkulltulaRewardOne/Two: the reward of the family member, not the
    // wallet the game picks by Link's.
    if (item == kGetItemWalletAdult || item == kGetItemWalletGiant || item == kGetItemRupeeGold) {
      if (source.ActorParams == kSkulltulaReward10)
        return kGetItemWalletAdult;
      if (source.ActorParams == kSkulltulaReward30)
        return kGetItemWalletGiant;
    }
    break;
  default:
    break;
  }
  return item;
}

} // namespace

std::optional<LocationKey> KeyForOffer(const OfferSource &source) {
  switch (source.ActorId) {
  case kActorChest:
    if (source.Scene == kSceneTreasureBoxShop &&
        ((source.ActorParams >> 5U) & 0x7FU) == kGetItemRupeePurpleLose)
      return std::nullopt;
    return LocationKey{source.Scene, OverrideType::Chest,
                       static_cast<std::uint8_t>(source.ActorParams & 0x1FU)};
  case kActorEnItem00: {
    const std::uint8_t type = static_cast<std::uint8_t>(source.ActorParams & 0xFFU);
    const std::uint16_t flag = source.CollectibleFlag;
    if (flag == 0U || flag >= 0x20U)
      return std::nullopt;
    if (type > kItem00RecoveryHeart && type != kItem00HeartPiece && type != kItem00SmallKey)
      return std::nullopt;
    return LocationKey{source.Scene, OverrideType::Collectable,
                       static_cast<std::uint8_t>(flag)};
  }
  case kActorSkullToken:
    return LocationKey{static_cast<std::uint8_t>((source.ActorParams >> 8U) & 0x1FU),
                       OverrideType::Skull,
                       static_cast<std::uint8_t>(source.ActorParams & 0xFFU)};
  case kActorDekuBaba:
  case kActorWitheredDekuBaba:
  // A shopkeeper offers what the shelf sells; shop slots report from their
  // buy event (ap_shops.h), and a shop item's get-item may equal a slot key.
  case kActorShopkeeper:
    return std::nullopt;
  default:
    // Zelda's Letter: a delayed location in the randomizer (play test
    // 2026-10-26: child Zelda offers it like any NPC).
    if (source.ActorId == kActorChildZelda && source.GetItemId == kGetItemZeldasLetter)
      return LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedZeldasLetter};
    if (source.Scene == kSceneGrottos && source.ActorId == kActorBusinessScrubSalesman)
      return LocationKey{source.RespawnReturnScene, OverrideType::GrottoScrub, RandomizerGetItem(source)};
    return LocationKey{source.Scene, OverrideType::BaseItem, RandomizerGetItem(source)};
  }
}

std::optional<LocationKey> KeyForDirectGive(std::uint8_t item) {
  // Saria's gift on the Lost Woods bridge, given in a cutscene (play test
  // 2026-10-26). The game gives no Fairy Ocarina anywhere else; one from the
  // server comes through an offer.
  if (item == kItemFairyOcarina)
    return LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedSariasGift};
  // The spells of the Great Fairies and the Light Arrows of the Temple of
  // Time cutscene: nothing else in the game gives them.
  switch (item) {
  case kItemFaroresWind:
    return LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedFairyZorasFountain};
  case kItemDinsFire:
    return LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedFairyHyruleCastle};
  case kItemNayrusLove:
    return LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedFairyColossus};
  case kItemLightArrows:
    return LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedLightArrowCutscene};
  default:
    break;
  }
  if (item >= kItemMedallionForest && item <= kItemZoraSapphire) {
    // Dungeon rewards: the stones in their boss rooms' warps, the medallions
    // from the sages, the Light Medallion from Rauru (Link's Pocket).
    if (item == kItemMedallionLight)
      return LocationKey{kTempleScene, OverrideType::Temple, kLinksPocketFlag};
    return LocationKey{kTempleScene, OverrideType::Temple,
                       item >= kItemKokiriEmerald
                           ? static_cast<std::uint8_t>(item - kItemKokiriEmerald)
                           : static_cast<std::uint8_t>(3U + (item - kItemMedallionForest))};
  }
  if (item < kItemSongMinuet || item > kItemSongStorms)
    return std::nullopt;
  return LocationKey{kDelayedScene, OverrideType::Delayed,
                     static_cast<std::uint8_t>(kDelayedSongMinuet + (item - kItemSongMinuet))};
}

std::vector<LocationKey> TokenKeys(std::span<const std::uint8_t> gsFlags) {
  std::vector<LocationKey> keys;
  for (std::size_t index = 0; index < gsFlags.size(); ++index)
    for (std::uint32_t bit = 0; bit < 8U; ++bit)
      if ((gsFlags[index] & (1U << bit)) != 0U)
        keys.push_back(LocationKey{static_cast<std::uint8_t>(index), OverrideType::Skull,
                                   static_cast<std::uint8_t>(1U << bit)});
  return keys;
}

std::optional<std::string_view> TeacherSong(const LocationKey &key) {
  // Delayed songs from kDelayedSongMinuet, in the order of their item ids.
  static constexpr std::array<std::string_view, 12> kSongs{
      "Minuet of Forest",   "Bolero of Fire",   "Serenade of Water", "Requiem of Spirit",
      "Nocturne of Shadow", "Prelude of Light", "Zelda's Lullaby",   "Epona's Song",
      "Saria's Song",       "Sun's Song",       "Song of Time",      "Song of Storms"};
  if (key.Scene != kDelayedScene || key.Type != OverrideType::Delayed || key.Flag < kDelayedSongMinuet ||
      key.Flag >= kDelayedSongMinuet + kSongs.size())
    return std::nullopt;
  return kSongs[key.Flag - kDelayedSongMinuet];
}

std::string Describe(const LocationKey &key) {
  static constexpr const char *kTypes[] = {
      "base", "chest", "collectable", "skull", "grotto_scrub", "delayed", "temple"};
  const auto type = static_cast<std::size_t>(key.Type);
  char text[96];
  std::snprintf(text, sizeof(text), "scene=0x%02X type=%s flag=0x%02X key=0x%08X",
                key.Scene, type < std::size(kTypes) ? kTypes[type] : "?", key.Flag,
                key.Packed());
  return text;
}

} // namespace Oot3dArchipelago
