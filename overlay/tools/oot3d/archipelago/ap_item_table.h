#pragma once

// How the game gives an Archipelago item of this world (Phase 4).
//
// The table is generated from OoT3D_Randomizer (recon/generate_tables.py,
// format triaevum_archipelago_items_v1): AP item id -> the randomizer's
// get-item id, and per get-item id the patch's item row (code/src/
// item_table.c): how a progressive item resolves against the save and what
// a custom item does. Get-item ids below 0x7C are the game's own; those the
// unmodified game gives with its own model, text and Item_Give. Custom ids
// are performed here by writing the save, as the randomizer's effects do.

#include "recomp/a32_runtime.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace Oot3dArchipelago {

inline constexpr std::uint16_t kFirstCustomGetItem = 0x7CU;
// Shown for another player's item and for items the game has no get-item
// for, until Phase 5 adds models: a recovery heart, which changes nothing
// lasting.
inline constexpr std::uint16_t kPlaceholderGetItem = 0x48U; // GI_HEART

enum class ItemUpgrade : std::uint8_t {
  None, Hookshot, Strength, BombBag, Quiver, BulletBag, Wallet, Scale, Nuts, Sticks, Magic,
  Bombchu, Ocarina, GoronSword, ArrowsToRupee, BombsToRupee, SeedsToRupee, TokenToRupee,
  HealthToRupee, LetterToBottle, Unknown,
};

enum class ItemEffect : std::uint8_t {
  None, GiveDungeonItem, GiveSmallKey, GiveSong, GiveMedallion, GiveStone, GiveMagic,
  GiveDoubleMagic, GiveBottle, GiveDefense, Other,
};

struct ItemRow {
  std::uint8_t ActionId = 0; // what Item_Give receives
  ItemUpgrade Upgrade = ItemUpgrade::None;
  ItemEffect Effect = ItemEffect::None;
  std::string EffectName;    // as in the randomizer, for messages
  std::optional<std::int32_t> Arg1, Arg2;
};

// Give through the game: Player takes the get-item and calls Item_Give.
struct GiveGetItem {
  std::uint16_t GetItemId = 0;
};
// Give by writing the save: the game has no get-item for it.
struct GiveDirect {
  std::uint16_t GetItemId = 0;
  ItemEffect Effect = ItemEffect::None;
  std::int32_t Arg1 = 0, Arg2 = 0;
};
// Not given: an effect the client does not perform (a feature outside the
// supported options), named for the message.
struct GiveUnsupported {
  std::string Reason;
};
using ItemPlan = std::variant<GiveGetItem, GiveDirect, GiveUnsupported>;

class ItemTable final {
public:
  static std::optional<ItemTable> Parse(const std::string &json, std::string *error);
  static std::optional<ItemTable> Load(const std::filesystem::path &path, std::string *error);

  // The get-item for an AP item with progressive items resolved against the
  // save the bus holds (OoT3D_Randomizer ItemTable_ResolveUpgrades), then
  // how to give it.
  [[nodiscard]] ItemPlan Plan(std::int64_t apItem, oot3d::recomp::a32::MemoryBus &memory) const;
  [[nodiscard]] std::optional<std::string> Name(std::int64_t apItem) const;
  [[nodiscard]] std::size_t Size() const noexcept { return mItems.size(); }

private:
  struct Item {
    std::string Name;
    std::uint16_t GetItemId = 0;
  };
  std::map<std::int64_t, Item> mItems;
  std::map<std::uint16_t, ItemRow> mRows;
};

// Performs a GiveDirect on the save. Returns false when guest memory is not
// writable (no save loaded).
bool ApplyDirect(const GiveDirect &give, oot3d::recomp::a32::MemoryBus &memory);

} // namespace Oot3dArchipelago
