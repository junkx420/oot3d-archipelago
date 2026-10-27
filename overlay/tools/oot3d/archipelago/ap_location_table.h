#pragma once

// AP location ids for OoT3D_Randomizer location keys.
//
// Id scheme (decided 2026-10-25): base + index in a generated, append-only
// table. The (scene, type, flag) key is stored beside each id. Master Quest
// dungeon locations can share a key with their vanilla counterpart, so a
// key maps to one id per dungeon variant.

#include "ap_location_key.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Oot3dArchipelago {

struct LocationEntry {
  std::int64_t Id = 0;
  std::string Name;
  LocationKey Key;
  bool MasterQuest = false;
};

class LocationTable final {
public:
  // Format "triaevum_archipelago_locations_v1". Rejects duplicate ids and
  // duplicate (key, master quest) pairs.
  static std::optional<LocationTable> Parse(const std::string &json, std::string *error);
  static std::optional<LocationTable> Load(const std::filesystem::path &path, std::string *error);

  // The id for a key. isMasterQuest tells whether the scene's dungeon is the
  // Master Quest variant in this seed (unknown scenes: vanilla).
  [[nodiscard]] std::optional<std::int64_t>
  Find(const LocationKey &key, const std::function<bool(std::uint8_t scene)> &isMasterQuest) const;
  [[nodiscard]] const LocationEntry *ById(std::int64_t id) const;
  // The vanilla entry of a location name (slot_data names locations).
  [[nodiscard]] const LocationEntry *ByName(const std::string &name) const;
  [[nodiscard]] std::size_t Size() const noexcept { return mEntries.size(); }

private:
  std::vector<LocationEntry> mEntries;
  // packed key -> {vanilla index, master quest index}
  std::map<std::uint32_t, std::pair<std::optional<std::size_t>, std::optional<std::size_t>>> mByKey;
  std::map<std::int64_t, std::size_t> mById;
};

} // namespace Oot3dArchipelago
