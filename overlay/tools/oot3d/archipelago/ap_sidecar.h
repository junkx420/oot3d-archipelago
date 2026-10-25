#pragma once

// Per-save AP progress (decided 2026-10-25): one JSON file per seed, AP slot
// and game save slot, beside the native saves. The native save file is never
// modified. The server stays the source of truth for received items: the
// sidecar remembers how many were applied, which checks were sent, what the
// slot's locations hold and which of this world's items were given where.

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace Oot3dArchipelago {

struct SidecarIdentity {
  std::string Seed;
  int Team = 0;
  int Slot = 0;
  int SaveSlot = 0;
  friend bool operator==(const SidecarIdentity &, const SidecarIdentity &) = default;
};

// What one of this slot's locations holds (from LocationScouts).
struct Placement {
  std::int64_t Item = 0; // id in the owner's game
  int Player = 0;        // owner slot
  unsigned Flags = 0;
  friend bool operator==(const Placement &, const Placement &) = default;
};

struct SidecarState {
  SidecarIdentity Identity;
  // Items with index < AppliedItems are in the game already.
  std::int64_t AppliedItems = 0;
  std::set<std::int64_t> ReportedLocations;
  // Kept so locations give their items without a connection.
  std::map<std::int64_t, Placement> Placements;
  // This world's items the game gave at their location itself; the
  // server's copy is skipped while the save still records the location.
  std::set<std::int64_t> GivenAtLocation;
  // This world's items the server gave (the location was not taken in this
  // save); their location then gives only a placeholder.
  std::set<std::int64_t> GivenByServer;
  // Ganon was defeated in this save; the goal is reported on every
  // connection until the server has it (it ignores repeats).
  bool GoalReached = false;
};

// <directory>/<seed>-<team>-<slot>-save<n>.json; the seed is reduced to
// [A-Za-z0-9_-] so the name is always a valid file name.
std::filesystem::path SidecarPath(const std::filesystem::path &directory,
                                  const SidecarIdentity &identity);

// A missing file is a fresh state. A file for another identity is an error
// and is left untouched (another seed in the same save slot).
std::optional<SidecarState> LoadSidecar(const std::filesystem::path &directory,
                                        const SidecarIdentity &identity, std::string *error);

// Writes a temporary file and renames it over the old one.
bool SaveSidecar(const std::filesystem::path &directory, const SidecarState &state,
                 std::string *error);

} // namespace Oot3dArchipelago
