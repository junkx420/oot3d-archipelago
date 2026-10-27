#include "ap_item_table.h"

#include "ap_shops.h"

#include "ap_guest_layout.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

constexpr const char *kFormat = "triaevum_archipelago_items_v1";

// Get-item ids the upgrades resolve to (shared/s_item.h at ae0fcf5).
constexpr std::uint16_t kGiHookshot = 0x08U, kGiLongshot = 0x09U;
constexpr std::uint16_t kGiBracelet = 0x54U, kGiGauntletsSilver = 0x35U, kGiGauntletsGold = 0x36U;
constexpr std::uint16_t kGiBombBag20 = 0x32U, kGiBombBag30 = 0x33U, kGiBombBag40 = 0x34U;
constexpr std::uint16_t kGiBow = 0x04U, kGiQuiver40 = 0x30U, kGiQuiver50 = 0x31U;
constexpr std::uint16_t kGiSlingshot = 0x05U, kGiBulletBag40 = 0x60U, kGiBulletBag50 = 0x7BU;
constexpr std::uint16_t kGiWalletAdult = 0x45U, kGiWalletGiant = 0x46U, kGiWalletTycoon = 0xC7U;
constexpr std::uint16_t kGiScaleSilver = 0x37U, kGiScaleGold = 0x38U;
constexpr std::uint16_t kGiNutUpgrade30 = 0x79U, kGiNutUpgrade40 = 0x7AU;
constexpr std::uint16_t kGiStickUpgrade20 = 0x77U, kGiStickUpgrade30 = 0x78U;
constexpr std::uint16_t kGiMagicSingle = 0xB9U, kGiMagicDouble = 0xBAU;
constexpr std::uint16_t kGiBombchus5 = 0x6AU, kGiBombchus10 = 0x03U, kGiBombchus20 = 0x6BU;
constexpr std::uint16_t kGiOcarinaFairy = 0x3BU, kGiOcarinaOot = 0x0CU;
constexpr std::uint16_t kGiSwordKnife = 0x28U, kGiSwordBgs = 0x57U;
constexpr std::uint16_t kGiRupeeBlue = 0x4DU, kGiRedundantLetterBottle = 0xC8U;

ItemUpgrade UpgradeFromName(const std::string &name) {
  static const std::map<std::string, ItemUpgrade> names = {
      {"None", ItemUpgrade::None}, {"Hookshot", ItemUpgrade::Hookshot},
      {"Strength", ItemUpgrade::Strength}, {"BombBag", ItemUpgrade::BombBag},
      {"Quiver", ItemUpgrade::Quiver}, {"BulletBag", ItemUpgrade::BulletBag},
      {"Wallet", ItemUpgrade::Wallet}, {"Scale", ItemUpgrade::Scale},
      {"Nuts", ItemUpgrade::Nuts}, {"Sticks", ItemUpgrade::Sticks},
      {"Magic", ItemUpgrade::Magic}, {"Bombchu", ItemUpgrade::Bombchu},
      {"Ocarina", ItemUpgrade::Ocarina}, {"GoronSword", ItemUpgrade::GoronSword},
      {"ArrowsToRupee", ItemUpgrade::ArrowsToRupee}, {"BombsToRupee", ItemUpgrade::BombsToRupee},
      {"SeedsToRupee", ItemUpgrade::SeedsToRupee}, {"TokenToRupee", ItemUpgrade::TokenToRupee},
      {"HealthToRupee", ItemUpgrade::HealthToRupee}, {"LetterToBottle", ItemUpgrade::LetterToBottle},
  };
  const auto found = names.find(name);
  return found != names.end() ? found->second : ItemUpgrade::Unknown;
}

ItemEffect EffectFromName(const std::string &name) {
  static const std::map<std::string, ItemEffect> names = {
      {"None", ItemEffect::None}, {"GiveDungeonItem", ItemEffect::GiveDungeonItem},
      {"GiveSmallKey", ItemEffect::GiveSmallKey}, {"GiveSong", ItemEffect::GiveSong},
      {"GiveMedallion", ItemEffect::GiveMedallion}, {"GiveStone", ItemEffect::GiveStone},
      {"GiveMagic", ItemEffect::GiveMagic}, {"GiveDoubleMagic", ItemEffect::GiveDoubleMagic},
      {"GiveBottle", ItemEffect::GiveBottle}, {"GiveDefense", ItemEffect::GiveDefense},
  };
  const auto found = names.find(name);
  return found != names.end() ? found->second : ItemEffect::Other;
}

std::optional<std::int32_t> Argument(const nlohmann::json &value) {
  if (value.is_number_integer())
    return value.get<std::int32_t>();
  return std::nullopt; // an enum the generator could not resolve
}

std::uint8_t Save8(MemoryBus &memory, std::uint32_t offset) {
  std::uint8_t value = 0;
  (void)memory.Read8(kSaveContext + offset, &value);
  return value;
}

std::uint16_t Save16(MemoryBus &memory, std::uint32_t offset) {
  std::uint16_t value = 0;
  (void)memory.Read16(kSaveContext + offset, &value);
  return value;
}

std::uint32_t Save32(MemoryBus &memory, std::uint32_t offset) {
  std::uint32_t value = 0;
  (void)memory.Read32(kSaveContext + offset, &value);
  return value;
}

// OoT3D_Randomizer code/src/item_upgrade.c.
std::uint16_t Upgrade(ItemUpgrade upgrade, std::uint16_t gi, MemoryBus &memory) {
  const std::uint32_t upgrades = Save32(memory, kSaveUpgrades);
  switch (upgrade) {
  case ItemUpgrade::None:
  case ItemUpgrade::Unknown:
    return gi;
  case ItemUpgrade::Hookshot:
    return Save8(memory, kSaveItems + kSlotHookshot) == kItemNone ? kGiHookshot : kGiLongshot;
  case ItemUpgrade::Strength:
    switch ((upgrades >> 6U) & 7U) {
    case 0: return kGiBracelet;
    case 1: return kGiGauntletsSilver;
    default: return kGiGauntletsGold;
    }
  case ItemUpgrade::BombBag:
    switch ((upgrades >> 3U) & 7U) {
    case 0: return kGiBombBag20;
    case 1: return kGiBombBag30;
    default: return kGiBombBag40;
    }
  case ItemUpgrade::Quiver:
    switch (upgrades & 7U) {
    case 0: return kGiBow;
    case 1: return kGiQuiver40;
    default: return kGiQuiver50;
    }
  case ItemUpgrade::BulletBag:
    switch ((upgrades >> 14U) & 7U) {
    case 0: return kGiSlingshot;
    case 1: return kGiBulletBag40;
    default: return kGiBulletBag50;
    }
  case ItemUpgrade::Wallet:
    switch ((upgrades >> 12U) & 3U) {
    case 0: return kGiWalletAdult;
    case 1: return kGiWalletGiant;
    default: return kGiWalletTycoon;
    }
  case ItemUpgrade::Scale:
    return ((upgrades >> 9U) & 7U) == 0U ? kGiScaleSilver : kGiScaleGold;
  case ItemUpgrade::Nuts:
    // 0 and 1 are both the starting capacity.
    return ((upgrades >> 20U) & 7U) <= 1U ? kGiNutUpgrade30 : kGiNutUpgrade40;
  case ItemUpgrade::Sticks:
    return ((upgrades >> 17U) & 7U) <= 1U ? kGiStickUpgrade20 : kGiStickUpgrade30;
  case ItemUpgrade::Magic:
    return Save8(memory, kSaveMagicAcquired) == 0U ? kGiMagicSingle : kGiMagicDouble;
  case ItemUpgrade::Bombchu:
    if (Save8(memory, kSaveItems + kSlotBombchu) == kItemNone)
      return kGiBombchus20;
    return static_cast<std::int8_t>(Save8(memory, kSaveAmmo + kSlotBombchu)) <= 5 ? kGiBombchus10
                                                                                   : kGiBombchus5;
  case ItemUpgrade::Ocarina:
    return Save8(memory, kSaveItems + kSlotOcarina) == kItemNone ? kGiOcarinaFairy : kGiOcarinaOot;
  case ItemUpgrade::GoronSword:
    return ((Save16(memory, kSaveEquipment) >> 2U) & 3U) == 0U ? kGiSwordKnife : kGiSwordBgs;
  case ItemUpgrade::ArrowsToRupee:
    return (upgrades & 7U) != 0U ? gi : kGiRupeeBlue;
  case ItemUpgrade::BombsToRupee:
    return ((upgrades >> 3U) & 7U) != 0U ? gi : kGiRupeeBlue;
  case ItemUpgrade::SeedsToRupee:
    return ((upgrades >> 14U) & 7U) != 0U ? gi : kGiRupeeBlue;
  case ItemUpgrade::TokenToRupee:
    return static_cast<std::int16_t>(Save16(memory, kSaveGsTokens)) < 100 ? gi : kGiRupeeBlue;
  case ItemUpgrade::HealthToRupee:
    return static_cast<std::int16_t>(Save16(memory, kSaveHealthCapacity)) < 320 ? gi : kGiRupeeBlue;
  case ItemUpgrade::LetterToBottle: {
    // eventChkInf[3] & 0x0008: King Zora moved aside.
    if ((Save16(memory, kSaveEventChkInf + 3U * 2U) & 0x0008U) != 0U)
      return kGiRedundantLetterBottle;
    for (std::uint8_t slot = kSlotBottle1; slot <= kSlotBottle4; ++slot)
      if (Save8(memory, kSaveItems + slot) == kItemLetterRuto)
        return kGiRedundantLetterBottle;
    return gi;
  }
  }
  return gi;
}

bool Write8(MemoryBus &memory, std::uint32_t offset, std::uint8_t value) {
  return memory.Write8(kSaveContext + offset, value);
}

bool Or32(MemoryBus &memory, std::uint32_t offset, std::uint32_t bits) {
  std::uint32_t value = 0;
  return memory.Read32(kSaveContext + offset, &value) &&
         memory.Write32(kSaveContext + offset, value | bits);
}

} // namespace

std::optional<ItemTable> ItemTable::Parse(const std::string &json, std::string *error) {
  const auto fail = [&](std::string message) -> std::optional<ItemTable> {
    if (error != nullptr)
      *error = std::move(message);
    return std::nullopt;
  };
  const auto document = nlohmann::json::parse(json, nullptr, false);
  if (document.is_discarded() || !document.is_object() || document.value("format", "") != kFormat)
    return fail(std::string("not a ") + kFormat + " table");
  ItemTable table;
  try {
    for (const auto &row : document.at("rows")) {
      ItemRow parsed;
      parsed.ActionId = row.at("action_id").get<std::uint8_t>();
      parsed.Upgrade = UpgradeFromName(row.at("upgrade").get<std::string>());
      parsed.EffectName = row.at("effect").get<std::string>();
      parsed.Effect = EffectFromName(parsed.EffectName);
      parsed.Arg1 = Argument(row.at("arg1"));
      parsed.Arg2 = Argument(row.at("arg2"));
      if (!table.mRows.emplace(row.at("gi").get<std::uint16_t>(), std::move(parsed)).second)
        return fail("duplicate item row " + row.at("gi").dump());
    }
    for (const auto &item : document.at("items")) {
      const auto id = item.at("id").get<std::int64_t>();
      if (!table.mItems.emplace(id, Item{item.at("name").get<std::string>(),
                                         item.at("gi").get<std::uint16_t>()}).second)
        return fail("duplicate item id " + std::to_string(id));
    }
  } catch (const nlohmann::json::exception &exception) {
    return fail(std::string("malformed item table: ") + exception.what());
  }
  return table;
}

std::optional<ItemTable> ItemTable::Load(const std::filesystem::path &path, std::string *error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (error != nullptr)
      *error = "cannot read " + path.string();
    return std::nullopt;
  }
  std::ostringstream text;
  text << input.rdbuf();
  return Parse(text.str(), error);
}

std::optional<std::string> ItemTable::Name(std::int64_t apItem) const {
  const auto found = mItems.find(apItem);
  return found != mItems.end() ? std::optional(found->second.Name) : std::nullopt;
}

ItemPlan ItemTable::Plan(std::int64_t apItem, MemoryBus &memory) const {
  const auto item = mItems.find(apItem);
  if (item == mItems.end())
    return GiveUnsupported{"unknown item id " + std::to_string(apItem)};
  // Shop ware ("Buy Deku Shield"): the table holds the shop item index; the
  // game's shop table names its get-item, given as it is (as the randomizer
  // gives its regular shop items).
  if (item->second.Name.starts_with("Buy ")) {
    if (const auto shopGi = ShopItemGetItem(item->second.GetItemId, memory))
      return GiveGetItem{*shopGi};
    return GiveUnsupported{item->second.Name + ": no shop item " + std::to_string(item->second.GetItemId)};
  }
  std::uint16_t gi = item->second.GetItemId;
  // ItemTable_ResolveUpgrades: follow upgrades until one returns its input.
  for (int step = 0; step < 8; ++step) {
    const auto row = mRows.find(gi);
    if (row == mRows.end())
      break;
    const std::uint16_t next = Upgrade(row->second.Upgrade, gi, memory);
    if (next == gi)
      break;
    gi = next;
  }
  if (gi < kFirstCustomGetItem)
    return GiveGetItem{gi};
  const auto row = mRows.find(gi);
  if (row == mRows.end())
    return GiveUnsupported{item->second.Name + ": no item row for get-item " + std::to_string(gi)};
  switch (row->second.Effect) {
  case ItemEffect::GiveDungeonItem:
  case ItemEffect::GiveSmallKey:
  case ItemEffect::GiveSong:
  case ItemEffect::GiveMedallion:
  case ItemEffect::GiveStone:
  case ItemEffect::GiveBottle:
    if (!row->second.Arg1.has_value())
      break;
    return GiveDirect{gi, row->second.Effect, *row->second.Arg1, row->second.Arg2.value_or(-1)};
  case ItemEffect::GiveMagic:
  case ItemEffect::GiveDoubleMagic:
  case ItemEffect::GiveDefense:
    return GiveDirect{gi, row->second.Effect, 0, 0};
  case ItemEffect::None:
  case ItemEffect::Other:
    break;
  }
  return GiveUnsupported{item->second.Name + ": effect " + row->second.EffectName +
                         " is not supported by the client"};
}

// OoT3D_Randomizer code/src/item_effect.c, without the randomizer-only
// extras (shared progress, skipped cutscenes).
bool ApplyDirect(const GiveDirect &give, MemoryBus &memory) {
  switch (give.Effect) {
  case ItemEffect::GiveDungeonItem: {
    // arg1: bit mask (boss key 1, compass 2, map 4), arg2: dungeon.
    if (give.Arg2 < 0 || give.Arg2 >= 20)
      return false;
    const std::uint32_t offset = kSaveDungeonItems + static_cast<std::uint32_t>(give.Arg2);
    return Write8(memory, offset, static_cast<std::uint8_t>(Save8(memory, offset) | give.Arg1));
  }
  case ItemEffect::GiveSmallKey: {
    if (give.Arg1 < 0 || give.Arg1 >= 19)
      return false;
    const std::uint32_t offset = kSaveDungeonKeys + static_cast<std::uint32_t>(give.Arg1);
    auto keys = static_cast<std::int8_t>(Save8(memory, offset));
    if (keys < 0)
      keys = 0;
    return Write8(memory, offset, static_cast<std::uint8_t>(keys + 1));
  }
  case ItemEffect::GiveSong:
    return Or32(memory, kSaveQuestItems, 1U << static_cast<std::uint32_t>(give.Arg1));
  case ItemEffect::GiveMedallion:
    return Or32(memory, kSaveQuestItems, static_cast<std::uint32_t>(give.Arg1));
  case ItemEffect::GiveStone:
    return Or32(memory, kSaveQuestItems, static_cast<std::uint32_t>(give.Arg1) << 16U);
  case ItemEffect::GiveMagic:
    return Write8(memory, kSaveMagicLevel, 1U) && Write8(memory, kSaveMagicAcquired, 1U) &&
           memory.Write16(kSaveContext + kSaveMagicMeterSize, 0x30U) &&
           Write8(memory, kSaveMagic, 0x30U);
  case ItemEffect::GiveDoubleMagic:
    return Write8(memory, kSaveMagicLevel, 2U) && Write8(memory, kSaveMagicAcquired, 1U) &&
           Write8(memory, kSaveDoubleMagic, 1U) &&
           memory.Write16(kSaveContext + kSaveMagicMeterSize, 0x60U) &&
           Write8(memory, kSaveMagic, 0x60U);
  case ItemEffect::GiveBottle:
    for (std::uint8_t slot = kSlotBottle1; slot <= kSlotBottle4; ++slot)
      if (Save8(memory, kSaveItems + slot) == kItemNone)
        return Write8(memory, kSaveItems + slot, static_cast<std::uint8_t>(give.Arg1));
    return true; // every bottle is full; the randomizer gives nothing either
  case ItemEffect::GiveDefense:
    if (!Write8(memory, kSaveDoubleDefense, 1U))
      return false;
    if (static_cast<std::int16_t>(Save16(memory, kSaveHealthCapacity)) > 0)
      return memory.Write16(kSaveContext + kSaveHealthAccumulator, 20U * 0x10U);
    return true;
  case ItemEffect::None:
  case ItemEffect::Other:
    break;
  }
  return false;
}

} // namespace Oot3dArchipelago
