#include "ap_shops.h"

#include "ap_guest_layout.h"

namespace Oot3dArchipelago {

using oot3d::recomp::a32::MemoryBus;

LocationKey ShopSlotKey(std::uint8_t scene, std::uint32_t position, std::int32_t entranceIndex) {
  std::uint32_t flag = kShopSlotFirstGetItem + position;
  if (scene == kSceneBazaar && entranceIndex == kEntranceKakarikoBazaar)
    flag += kShopSlotsPerShop;
  return LocationKey{scene, OverrideType::BaseItem, static_cast<std::uint8_t>(flag)};
}

std::optional<std::uint16_t> ShopItemGetItem(std::uint16_t shopItem, MemoryBus &memory) {
  std::uint32_t getItem = 0;
  if (shopItem >= kShopItemEntryCount ||
      !memory.Read32(kShopItemEntries + shopItem * kShopItemEntrySize + kShopItemEntryGetItem, &getItem) ||
      getItem == 0U || getItem > 0xFFU)
    return std::nullopt;
  return static_cast<std::uint16_t>(getItem);
}

bool ApplyShopSlot(std::uint32_t actor, const ShopSlotItem &item, MemoryBus &memory) {
  return memory.Write32(actor + kGirlGetItemId, item.GetItemId) &&
         memory.Write16(actor + kGirlBasePrice, static_cast<std::uint16_t>(item.Price)) &&
         memory.Write16(actor + kGirlItemCount, 1U) && memory.Write32(actor + kGirlItemGiveFunc, 0U) &&
         memory.Write32(actor + kGirlBuyEventFunc, kShopBuyEventDefault);
}

bool SellOutShopSlot(std::uint32_t actor, MemoryBus &memory) {
  return memory.Write16(actor + kActorParams, kShopItemSoldOut) &&
         memory.Write32(actor + kGirlActionFunc2, kEnGirlAInitializeItemAction);
}

} // namespace Oot3dArchipelago
