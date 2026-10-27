#include "ap_location_table.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace Oot3dArchipelago {
namespace {

constexpr const char *kFormat = "triaevum_archipelago_locations_v1";

bool Fail(std::string *error, std::string message) {
  if (error != nullptr)
    *error = std::move(message);
  return false;
}

} // namespace

std::optional<LocationTable> LocationTable::Parse(const std::string &text, std::string *error) {
  const auto document = nlohmann::json::parse(text, nullptr, false);
  if (document.is_discarded() || !document.is_object()) {
    Fail(error, "location table: not a JSON object");
    return std::nullopt;
  }
  if (document.value("format", "") != kFormat) {
    Fail(error, std::string("location table: format is not ") + kFormat);
    return std::nullopt;
  }
  const auto locations = document.find("locations");
  if (locations == document.end() || !locations->is_array()) {
    Fail(error, "location table: missing locations array");
    return std::nullopt;
  }
  LocationTable table;
  for (const auto &row : *locations) {
    if (!row.is_object() || !row.contains("id") || !row.contains("scene") ||
        !row.contains("type") || !row.contains("flag")) {
      Fail(error, "location table: a row lacks id, scene, type or flag");
      return std::nullopt;
    }
    LocationEntry entry;
    entry.Id = row.at("id").get<std::int64_t>();
    entry.Name = row.value("name", "");
    const int type = row.at("type").get<int>();
    if (type < 0 || type > static_cast<int>(OverrideType::Temple)) {
      Fail(error, "location table: unknown override type in " + entry.Name);
      return std::nullopt;
    }
    entry.Key = {row.at("scene").get<std::uint8_t>(), static_cast<OverrideType>(type),
                 row.at("flag").get<std::uint8_t>()};
    entry.MasterQuest = row.value("master_quest", false);
    if (table.mById.count(entry.Id) != 0U) {
      Fail(error, "location table: duplicate id " + std::to_string(entry.Id));
      return std::nullopt;
    }
    auto &slot = table.mByKey[entry.Key.Packed()];
    auto &variant = entry.MasterQuest ? slot.second : slot.first;
    if (variant.has_value()) {
      Fail(error, "location table: duplicate key for " + entry.Name);
      return std::nullopt;
    }
    variant = table.mEntries.size();
    table.mById[entry.Id] = table.mEntries.size();
    table.mEntries.push_back(std::move(entry));
  }
  return table;
}

std::optional<LocationTable> LocationTable::Load(const std::filesystem::path &path,
                                                 std::string *error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    Fail(error, "location table: cannot read " + path.string());
    return std::nullopt;
  }
  std::ostringstream text;
  text << input.rdbuf();
  return Parse(text.str(), error);
}

std::optional<std::int64_t>
LocationTable::Find(const LocationKey &key,
                    const std::function<bool(std::uint8_t scene)> &isMasterQuest) const {
  const auto found = mByKey.find(key.Packed());
  if (found == mByKey.end())
    return std::nullopt;
  const bool masterQuest = isMasterQuest && isMasterQuest(key.Scene);
  const auto &index = masterQuest ? found->second.second : found->second.first;
  if (!index.has_value())
    return std::nullopt;
  return mEntries[*index].Id;
}

const LocationEntry *LocationTable::ById(std::int64_t id) const {
  const auto found = mById.find(id);
  return found == mById.end() ? nullptr : &mEntries[found->second];
}

const LocationEntry *LocationTable::ByName(const std::string &name) const {
  for (const LocationEntry &entry : mEntries)
    if (!entry.MasterQuest && entry.Name == name)
      return &entry;
  return nullptr;
}

} // namespace Oot3dArchipelago
