#include "ap_item_receiver.h"

#include "ap_guest_layout.h"

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

constexpr std::uint32_t kItemActor = kClientRegionBase;

std::uint32_t Read32(MemoryBus &memory, std::uint32_t address) {
  std::uint32_t value = 0;
  return address != 0U && memory.Read32(address, &value) ? value : 0U;
}

std::uint16_t Read16(MemoryBus &memory, std::uint32_t address) {
  std::uint16_t value = 0;
  return address != 0U && memory.Read16(address, &value) ? value : 0U;
}

std::uint8_t Read8(MemoryBus &memory, std::uint32_t address) {
  std::uint8_t value = 0;
  return address != 0U && memory.Read8(address, &value) ? value : 0U;
}

std::uint32_t Play(MemoryBus &memory) { return Read32(memory, kGlobalContextPointer); }

} // namespace

std::uint32_t PlayerActor(MemoryBus &memory) {
  const std::uint32_t play = Play(memory);
  return play != 0U ? Read32(memory, play + kPlayPlayerActor) : 0U;
}

bool PlayerCanTakeItem(MemoryBus &memory) {
  const std::uint32_t play = Play(memory);
  const std::uint32_t player = PlayerActor(memory);
  if (play == 0U || player == 0U)
    return false;
  if ((Read32(memory, player + kPlayerStateFlags1) & kPlayerState1Busy) != 0U ||
      (Read32(memory, player + kPlayerStateFlags2) & kPlayerState2Busy) != 0U)
    return false;
  // Busy with another actor's item or interaction; our own offer from the
  // previous frame may still be standing.
  const std::uint32_t interacting = Read32(memory, player + kPlayerInteractRangeActor);
  if (interacting != kItemActor &&
      (interacting != 0U || Read8(memory, player + kPlayerGetItemId) != 0U))
    return false;
  if (Read32(memory, player + kActorDraw) == 0U)
    return false;
  if (Read8(memory, play + kPlayTitleDelay) != 0U || Read8(memory, play + kPlayTitleDuration) != 0U ||
      Read8(memory, play + kPlayTitleAlpha) != 0U || Read8(memory, play + kPlaySceneLoadFlag) != 0U)
    return false;
  const bool onGround = (Read16(memory, player + kActorBgCheckFlags) & kBgCheckGround) != 0U;
  const bool submerged = (Read32(memory, player + kPlayerStateFlags1) & kPlayerState1Submerged) != 0U;
  return onGround && !submerged;
}

std::optional<bool> LocationTakenInSave(const LocationKey &key, MemoryBus &memory) {
  std::uint32_t current = 0;
  std::uint32_t saved = 0;
  switch (key.Type) {
  case OverrideType::Chest:
    current = kPlayChestFlags;
    saved = kSceneFlagsChest;
    break;
  case OverrideType::Collectable:
    current = kPlayCollectFlags;
    saved = kSceneFlagsCollect;
    break;
  default:
    return std::nullopt;
  }
  if (key.Flag >= 32U || key.Scene >= kSaveSceneCount)
    return std::nullopt;
  const std::uint32_t play = Play(memory);
  const bool inScene = play != 0U && Read16(memory, play + kPlaySceneNum) == key.Scene;
  const std::uint32_t flags =
      inScene ? Read32(memory, play + current)
              : Read32(memory, kSaveContext + kSaveSceneFlags + key.Scene * kSceneFlagsSize + saved);
  return ((flags >> key.Flag) & 1U) != 0U;
}

bool ItemReceiver::EnsureActor(MemoryBus &memory, std::string *error) {
  if (mActorReady)
    return true;
  if (mUnavailable)
    return false;
  std::uint8_t probe = 0;
  if (!memory.Read8(kItemActor, &probe)) {
    if (!mMapper || !mMapper(kClientRegionBase, kClientRegionSize, error)) {
      mUnavailable = true;
      return false;
    }
  }
  for (std::uint32_t offset = 0; offset < kActorSize; offset += 4U)
    (void)memory.Write32(kItemActor + offset, 0U);
  // The minimum Player needs to hand an item over (ItemOverride_Init).
  (void)memory.Write32(kItemActor + kActorUpdate, 1U);
  mActorReady = true;
  return true;
}

ItemReceiver::Result ItemReceiver::Give(std::uint16_t getItemId, MemoryBus &memory, std::string *error) {
  if (!EnsureActor(memory, error))
    return Result::Unavailable;
  if (mOffered && Read32(memory, kItemActor + kActorParent) != 0U) {
    // Taken on the previous frame: Player set itself as the parent.
    (void)memory.Write32(kItemActor + kActorParent, 0U);
    mOffered = false;
    return Result::Given;
  }
  if (!PlayerCanTakeItem(memory))
    return Result::Waiting;
  const std::uint32_t player = PlayerActor(memory);
  (void)memory.Write32(player + kPlayerInteractRangeActor, kItemActor);
  (void)memory.Write8(player + kPlayerGetItemId, static_cast<std::uint8_t>(getItemId));
  mOffered = true;
  return Result::Waiting;
}

void ItemReceiver::Cancel(MemoryBus &memory) {
  if (!mOffered)
    return;
  const std::uint32_t player = PlayerActor(memory);
  if (player != 0U && Read32(memory, player + kPlayerInteractRangeActor) == kItemActor) {
    (void)memory.Write32(player + kPlayerInteractRangeActor, 0U);
    (void)memory.Write8(player + kPlayerGetItemId, 0U);
  }
  mOffered = false;
}

} // namespace Oot3dArchipelago
