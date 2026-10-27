#include "ap_location_key.h"

#include <cstdio>

namespace Oot3dArchipelago {

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
    return std::nullopt;
  default:
    // Zelda's Letter: a delayed location in the randomizer (play test
    // 2026-10-26: child Zelda offers it like any NPC).
    if (source.ActorId == kActorChildZelda && source.GetItemId == kGetItemZeldasLetter)
      return LocationKey{kDelayedScene, OverrideType::Delayed, kDelayedZeldasLetter};
    if (source.Scene == kSceneGrottos && source.ActorId == kActorBusinessScrubSalesman)
      return LocationKey{source.RespawnReturnScene, OverrideType::GrottoScrub,
                         source.GetItemId};
    return LocationKey{source.Scene, OverrideType::BaseItem, source.GetItemId};
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
