#include "ap_save_sync.h"

#include "ap_guest_layout.h"

#include <string>

namespace Oot3dArchipelago {

bool IsNewFile(oot3d::recomp::a32::MemoryBus &memory) {
  std::uint16_t scene = 0;
  std::uint32_t questItems = 0;
  if (!memory.Read16(kSaveContext + kSaveSceneIndex, &scene) || scene != kSceneLinksHouse ||
      !memory.Read32(kSaveContext + kSaveQuestItems, &questItems) || questItems != 0U)
    return false;
  for (std::uint32_t slot = 0; slot < 24U; ++slot) {
    std::uint8_t item = 0;
    if (!memory.Read8(kSaveContext + kSaveItems + slot, &item) || item != kItemNone)
      return false;
  }
  return true;
}

std::uint32_t IdentityHash(const SidecarIdentity &identity) {
  const std::string text = identity.Seed + '/' + std::to_string(identity.Team) + '/' +
                           std::to_string(identity.Slot) + '/' + std::to_string(identity.SaveSlot);
  std::uint32_t hash = 2166136261U;
  for (const unsigned char byte : text) {
    hash ^= byte;
    hash *= 16777619U;
  }
  return hash;
}

std::optional<ProgressRecord> ReadProgressRecord(oot3d::recomp::a32::MemoryBus &memory) {
  std::uint32_t magic = 0;
  ProgressRecord record;
  if (!memory.Read32(kClientProgressRecord, &magic) || magic != kProgressRecordMagic ||
      !memory.Read32(kClientProgressRecord + 4U, &record.Identity) ||
      !memory.Read32(kClientProgressRecord + 8U, &record.AppliedItems))
    return std::nullopt;
  return record;
}

bool WriteProgressRecord(oot3d::recomp::a32::MemoryBus &memory, const ProgressRecord &record) {
  return memory.Write32(kClientProgressRecord + 4U, record.Identity) &&
         memory.Write32(kClientProgressRecord + 8U, record.AppliedItems) &&
         memory.Write32(kClientProgressRecord, kProgressRecordMagic);
}

ProgressDecision DecideProgress(const SidecarState &state, bool enteredGameplay, bool newFile,
                                const std::optional<ProgressRecord> &record) {
  if (enteredGameplay)
    return newFile ? ProgressDecision{ProgressEvent::NewFile, 0}
                   : ProgressDecision{ProgressEvent::LoadedFromDisk, state.AppliedAtLastSave};
  if (record.has_value() && record->Identity == IdentityHash(state.Identity) &&
      static_cast<std::int64_t>(record->AppliedItems) != state.AppliedItems)
    return {ProgressEvent::Savestate, static_cast<std::int64_t>(record->AppliedItems)};
  return {ProgressEvent::None, state.AppliedItems};
}

} // namespace Oot3dArchipelago
