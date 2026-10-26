#pragma once

// Keeps the applied-item count (SidecarState::AppliedItems) in step with
// what the game's memory holds. The native save file is never modified, so
// three things can leave the game behind the count:
//
// - Loading a file from disk: items given after the game last saved are not
//   in it. The session notes the count whenever the game writes the save
//   file (AppliedAtLastSave) and goes back to it when gameplay starts from
//   the file selection.
// - A new file in the same save slot: nothing is in it (IsNewFile).
// - Loading a savestate: guest memory, the client page included, returns to
//   that moment. The page holds a progress record with the count of the
//   moment, so the count follows it.

#include "ap_sidecar.h"
#include "recomp/a32_runtime.h"

#include <cstdint>
#include <optional>

namespace Oot3dArchipelago {

// A file the game has just created: in Link's house without any item or
// quest item. Returning players always carry something.
bool IsNewFile(oot3d::recomp::a32::MemoryBus &memory);

// FNV-1a of the sidecar identity: a record of another seed, slot or save
// slot is never adopted.
std::uint32_t IdentityHash(const SidecarIdentity &identity);

struct ProgressRecord {
  std::uint32_t Identity = 0;
  std::uint32_t AppliedItems = 0;
};

// The client page must be mapped (EnsureClientPage); a page without the
// magic, or not mapped, has no record.
std::optional<ProgressRecord> ReadProgressRecord(oot3d::recomp::a32::MemoryBus &memory);
bool WriteProgressRecord(oot3d::recomp::a32::MemoryBus &memory, const ProgressRecord &record);

// What the count must become, given what happened since the last frame.
enum class ProgressEvent : std::uint8_t { None, NewFile, LoadedFromDisk, Savestate };
struct ProgressDecision {
  ProgressEvent Event = ProgressEvent::None;
  std::int64_t AppliedItems = 0;
};
// enteredGameplay: gameplay started from the file selection this frame, or
// the sidecar opened for the first time in this run while in gameplay.
ProgressDecision DecideProgress(const SidecarState &state, bool enteredGameplay, bool newFile,
                                const std::optional<ProgressRecord> &record);

} // namespace Oot3dArchipelago
