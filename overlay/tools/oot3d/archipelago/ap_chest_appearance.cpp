#include "ap_chest_appearance.h"

#include "ap_guest_layout.h"

#include <bit>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

// The chest model's mesh groups (chest.c): 0 fancy chest, 1 wooden chest,
// 2 fancy lid, 3 wooden lid.
constexpr std::uint32_t kMeshFancyChest = 0U;
constexpr std::uint32_t kMeshWoodenChest = 1U;
constexpr std::uint32_t kMeshFancyLid = 2U;
constexpr std::uint32_t kMeshWoodenLid = 3U;
// Model_EnableMeshGroupByIndex (EUR 0x0037266C, body 0x004C8B8C): the model's
// +0x14 points at the mesh group state, whose +0x68 is the group count and
// +0x6C the list of one byte per group.
constexpr std::uint32_t kModelMeshState = 0x14U;
constexpr std::uint32_t kMeshStateCount = 0x68U;
constexpr std::uint32_t kMeshStateList = 0x6CU;

constexpr float kBigScale = 0.01f;
constexpr float kSmallScale = 0.005f;

constexpr std::uint8_t kSceneDekuTree = 0x00U;
constexpr std::uint8_t kSceneSpiritTemple = 0x06U;
constexpr std::uint8_t kSceneGanonsCastle = 0x0DU;
constexpr std::int8_t kTreasureBoxShopFinalRoom = 6;

bool WriteFloat(MemoryBus &memory, std::uint32_t address, float value) {
  return memory.Write32(address, std::bit_cast<std::uint32_t>(value));
}

bool SetMeshGroup(MemoryBus &memory, std::uint32_t state, std::uint32_t index, bool enabled) {
  std::uint32_t count = 0;
  std::uint32_t list = 0;
  if (!memory.Read32(state + kMeshStateCount, &count) || !memory.Read32(state + kMeshStateList, &list))
    return false;
  // The game's signed compare: a group past the count is left alone.
  if (static_cast<std::int32_t>(count) <= static_cast<std::int32_t>(index))
    return true;
  return memory.Write8(list + index, enabled ? 1U : 0U);
}

} // namespace

const char *ToString(ChestAppearance appearance) noexcept {
  return appearance == ChestAppearance::Classic ? "classic" : "vanilla";
}

std::optional<ChestAppearance> ParseChestAppearance(std::string_view text) noexcept {
  if (text == "vanilla")
    return ChestAppearance::Vanilla;
  if (text == "classic")
    return ChestAppearance::Classic;
  return std::nullopt;
}

ChestContents ClassifyChestContents(std::string_view name, unsigned flags) noexcept {
  if (name.ends_with("Big Key"))
    return ChestContents::BossKey;
  if (name.ends_with("Small Key") || name.ends_with("Key Ring"))
    return ChestContents::SmallKey;
  // The randomizer's CHEST_HEART and CHEST_SKULL items stay small whatever
  // the logic needs of them.
  if (name == "Heart Container" || name == "Piece of Heart" || name.starts_with("Piece of Heart (") ||
      name == "Gold Skulltula Token")
    return ChestContents::Minor;
  return (flags & (kItemFlagProgression | kItemFlagTrap)) != 0U ? ChestContents::Major : ChestContents::Minor;
}

std::optional<LocationKey> ChestKey(std::uint16_t params, std::uint8_t scene) {
  OfferSource source;
  source.ActorId = kActorChest;
  source.ActorParams = params;
  source.Scene = scene;
  source.GetItemId = static_cast<std::uint8_t>((params >> 5U) & 0x7FU);
  return KeyForOffer(source);
}

bool ApplyChestAppearance(std::uint32_t actor, std::uint8_t scene, ChestContents contents, MemoryBus &memory) {
  std::uint8_t room = 0;
  std::uint16_t params = 0;
  std::uint32_t model = 0;
  std::uint32_t state = 0;
  if (!memory.Read8(actor + kActorRoom, &room) || !memory.Read16(actor + kActorParams, &params) ||
      !memory.Read32(actor + kEnBoxModel, &model) || model == 0U ||
      !memory.Read32(model + kModelMeshState, &state) || state == 0U)
    return false;
  // The treasure chest game keeps its chests but in the last room.
  if (scene == kSceneTreasureBoxShop && static_cast<std::int8_t>(room) != kTreasureBoxShopFinalRoom)
    return false;

  const bool fancy = contents == ChestContents::BossKey || contents == ChestContents::SmallKey;
  bool written = SetMeshGroup(memory, state, fancy ? kMeshFancyChest : kMeshWoodenChest, true) &&
                 SetMeshGroup(memory, state, fancy ? kMeshFancyLid : kMeshWoodenLid, true) &&
                 SetMeshGroup(memory, state, fancy ? kMeshWoodenChest : kMeshFancyChest, false) &&
                 SetMeshGroup(memory, state, fancy ? kMeshWoodenLid : kMeshFancyLid, false);

  const bool big = contents == ChestContents::Major || contents == ChestContents::BossKey;
  const float scale = big ? kBigScale : kSmallScale;
  for (std::uint32_t axis = 0; axis < 3U; ++axis)
    written = WriteFloat(memory, actor + kActorScale + axis * 4U, scale) && written;

  const std::uint32_t x = actor + kActorWorldPos;
  const std::uint32_t z = actor + kActorWorldPos + 8U;
  const auto signedRoom = static_cast<std::int8_t>(room);
  if (big) {
    // Ganon's Castle's Zelda's Lullaby chest, moved back so it is reachable.
    if ((params & 0xF000U) == 0x8000U && scene == kSceneGanonsCastle && signedRoom == 9)
      written = WriteFloat(memory, z, -962.0f) && written;
    // Master Quest Deku Tree's Song of Time chest.
    if (params == 0x5AA0U && scene == kSceneDekuTree && signedRoom == 5)
      written = WriteFloat(memory, x, -1380.0f) && written;
  } else if (params == 0x36C5U && scene == kSceneGanonsCastle && signedRoom == 12) {
    // Ganon's Castle's Gold Gauntlets chest, in hookshot reach of the switch
    // platform.
    written = WriteFloat(memory, x, 1777.0f) && WriteFloat(memory, z, -3626.0f) && written;
  } else if (params == 0x3804U && scene == kSceneSpiritTemple && signedRoom == 14) {
    // The Spirit Temple's compass chest, in hookshot reach.
    written = WriteFloat(memory, x, 400.0f) && written;
  }
  return written;
}

} // namespace Oot3dArchipelago
