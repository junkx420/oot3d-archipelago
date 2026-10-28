#pragma once

// Shop slots as locations (Ship of Harkinian's shuffle_shops), after
// OoT3D_Randomizer's shops.c, which replaces the shop item actor (EnGirlA)
// with its own. The client keeps the game's actor and changes its fields
// instead, every frame it is drawn: the get-item it sells, the price from the
// seed, a single count, no direct give and the default buy event, which only
// takes the price. The shopkeeper's test of the item (EnGirlA.canBuyFunc) is
// answered in the block after each call: enough rupees or not. A bought slot
// reports its location and is sold out, as the randomizer's slots are. The
// shelf still shows the game's own item and text.
//
// Structure offsets: OoT3D_Randomizer code/src/actors/shops.h (EnGirlA,
// ShopItemEntry); the table and the hooked blocks: addresses.json.

#include "ap_guest_layout.h"
#include "ap_location_key.h"
#include "recomp/a32_runtime.h"

#include <cstdint>
#include <optional>

namespace Oot3dArchipelago {

inline constexpr std::uint32_t kShopItemEntries = 0x00524F50U; // EnGirlA_ShopItemEntries
inline constexpr std::uint32_t kShopItemEntrySize = 0x30U;
inline constexpr std::uint32_t kShopItemEntryGetItem = 0x14U; // s32
inline constexpr std::uint32_t kShopItemEntryCount = 50U;
inline constexpr std::uint32_t kEnGirlAInitializeItemAction = 0x0014D5C8U;
inline constexpr std::uint32_t kShopBuyEventDefault = 0x00114C04U;

// EnGirlA fields.
inline constexpr std::uint32_t kGirlActionFunc2 = 0x1ACU;
inline constexpr std::uint32_t kGirlGetItemId = 0x1B8U; // s32
inline constexpr std::uint32_t kGirlCanBuyFunc = 0x1D0U;
inline constexpr std::uint32_t kGirlItemGiveFunc = 0x1D4U;
inline constexpr std::uint32_t kGirlBuyEventFunc = 0x1D8U;
inline constexpr std::uint32_t kGirlBasePrice = 0x1DCU; // s16
inline constexpr std::uint32_t kGirlItemCount = 0x1DEU; // s16
inline constexpr std::uint16_t kShopItemSoldOut = 38U;  // SI_SOLD_OUT

// EnGirlA_CanBuyResult.
inline constexpr std::uint32_t kCanBuySuccessFanfare = 0U;
inline constexpr std::uint32_t kCanBuyNeedRupees = 4U;

inline constexpr std::uint32_t kSaveEntranceIndex = 0x0000U; // s32
inline constexpr std::uint8_t kSceneBazaar = 0x2CU;
// The Kakariko Bazaar shares the Market Bazaar's scene; its entrance (0x00B7)
// tells them apart, and its slots follow the market's (0x38 on).
inline constexpr std::int32_t kEntranceKakarikoBazaar = 0x00B7;
inline constexpr std::uint8_t kShopSlotFirstGetItem = 0x30U;
inline constexpr std::uint32_t kShopSlotsPerShop = 8U;

// The location of a shop's slot: base item 0x30 + its position (+ 8 in the
// Kakariko Bazaar), as ShopsanityItem_Init numbers them.
LocationKey ShopSlotKey(std::uint8_t scene, std::uint32_t position, std::int32_t entranceIndex);

// The get-item a "Buy ..." item stands for: the item tables give such items
// the shop item index (EnGirlA_ShopItems), the game's table its get-item.
std::optional<std::uint16_t> ShopItemGetItem(std::uint16_t shopItem, oot3d::recomp::a32::MemoryBus &memory);

// What a slot sells instead of the game's item.
struct ShopSlotItem {
  std::uint16_t GetItemId = 0;
  std::int16_t Price = 0;
};

// Writes the slot's fields (see above); false if the actor cannot be written.
bool ApplyShopSlot(std::uint32_t actor, const ShopSlotItem &item, oot3d::recomp::a32::MemoryBus &memory);
// Sells the slot out: the game reinitializes it as SI_SOLD_OUT.
bool SellOutShopSlot(std::uint32_t actor, oot3d::recomp::a32::MemoryBus &memory);

} // namespace Oot3dArchipelago
