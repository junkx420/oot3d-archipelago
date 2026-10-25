#include "ap_sidecar.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <fstream>
#include <sstream>

namespace Oot3dArchipelago {
namespace {

constexpr const char *kFormat = "triaevum_archipelago_sidecar_v1";

bool Fail(std::string *error, std::string message) {
  if (error != nullptr)
    *error = std::move(message);
  return false;
}

std::string SafeName(const std::string &text) {
  std::string safe;
  for (const char c : text)
    safe += (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') ? c : '_';
  return safe.empty() ? "unknown" : safe;
}

} // namespace

std::filesystem::path SidecarPath(const std::filesystem::path &directory,
                                  const SidecarIdentity &identity) {
  return directory / (SafeName(identity.Seed) + "-" + std::to_string(identity.Team) + "-" +
                      std::to_string(identity.Slot) + "-save" +
                      std::to_string(identity.SaveSlot) + ".json");
}

std::optional<SidecarState> LoadSidecar(const std::filesystem::path &directory,
                                        const SidecarIdentity &identity, std::string *error) {
  const auto path = SidecarPath(directory, identity);
  SidecarState state;
  state.Identity = identity;
  std::error_code exists;
  if (!std::filesystem::exists(path, exists))
    return state;
  std::ifstream input(path, std::ios::binary);
  std::ostringstream text;
  text << input.rdbuf();
  const auto document = nlohmann::json::parse(text.str(), nullptr, false);
  if (document.is_discarded() || document.value("format", "") != kFormat) {
    Fail(error, "Archipelago save data is unreadable: " + path.string());
    return std::nullopt;
  }
  const SidecarIdentity stored{document.value("seed", ""), document.value("team", -1),
                               document.value("slot", -1), document.value("save_slot", -1)};
  if (!(stored == identity)) {
    Fail(error, "Archipelago save data belongs to another seed or slot: " + path.string());
    return std::nullopt;
  }
  state.AppliedItems = document.value("applied_items", std::int64_t{0});
  for (const auto &location : document.value("reported_locations", nlohmann::json::array()))
    state.ReportedLocations.insert(location.get<std::int64_t>());
  return state;
}

bool SaveSidecar(const std::filesystem::path &directory, const SidecarState &state,
                 std::string *error) {
  std::error_code created;
  std::filesystem::create_directories(directory, created);
  const auto path = SidecarPath(directory, state.Identity);
  const nlohmann::json document = {
      {"format", kFormat},
      {"seed", state.Identity.Seed},
      {"team", state.Identity.Team},
      {"slot", state.Identity.Slot},
      {"save_slot", state.Identity.SaveSlot},
      {"applied_items", state.AppliedItems},
      {"reported_locations", state.ReportedLocations},
  };
  auto temporary = path;
  temporary += ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output << document.dump(1) << '\n';
    if (!output)
      return Fail(error, "cannot write " + temporary.string());
  }
  std::error_code renamed;
  std::filesystem::rename(temporary, path, renamed);
  if (renamed)
    return Fail(error, "cannot replace " + path.string() + ": " + renamed.message());
  return true;
}

} // namespace Oot3dArchipelago
