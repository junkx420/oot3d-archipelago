#include "ap_sidecar.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

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
  try {
    state.AppliedItems = document.value("applied_items", std::int64_t{0});
    for (const auto &location : document.value("reported_locations", nlohmann::json::array()))
      state.ReportedLocations.insert(location.get<std::int64_t>());
    // Phase 4 fields; files from before have none.
    for (const auto &row : document.value("placements", nlohmann::json::array()))
      state.Placements[row.at(0).get<std::int64_t>()] =
          Placement{row.at(1).get<std::int64_t>(), row.at(2).get<int>(), row.at(3).get<unsigned>()};
    for (const auto &location : document.value("given_at_location", nlohmann::json::array()))
      state.GivenAtLocation.insert(location.get<std::int64_t>());
    for (const auto &location : document.value("given_by_server", nlohmann::json::array()))
      state.GivenByServer.insert(location.get<std::int64_t>());
    state.GoalReached = document.value("goal_reached", false);
    // Files from before keep their count as the saved one.
    state.AppliedAtLastSave = document.value("applied_at_last_save", state.AppliedItems);
  } catch (const nlohmann::json::exception &) {
    Fail(error, "Archipelago save data is unreadable: " + path.string());
    return std::nullopt;
  }
  return state;
}

bool SaveSidecar(const std::filesystem::path &directory, const SidecarState &state,
                 std::string *error) {
  std::error_code created;
  std::filesystem::create_directories(directory, created);
  const auto path = SidecarPath(directory, state.Identity);
  nlohmann::json placements = nlohmann::json::array();
  for (const auto &[location, placement] : state.Placements)
    placements.push_back({location, placement.Item, placement.Player, placement.Flags});
  const nlohmann::json document = {
      {"format", kFormat},
      {"seed", state.Identity.Seed},
      {"team", state.Identity.Team},
      {"slot", state.Identity.Slot},
      {"save_slot", state.Identity.SaveSlot},
      {"applied_items", state.AppliedItems},
      {"applied_at_last_save", state.AppliedAtLastSave},
      {"reported_locations", state.ReportedLocations},
      {"placements", placements},
      {"given_at_location", state.GivenAtLocation},
      {"given_by_server", state.GivenByServer},
      {"goal_reached", state.GoalReached},
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

SidecarWriter::SidecarWriter(std::filesystem::path directory)
    : mDirectory(std::move(directory)), mThread([this] { Run(); }) {}

SidecarWriter::~SidecarWriter() {
  {
    std::lock_guard lock(mMutex);
    mStop = true;
  }
  mWake.notify_one();
  if (mThread.joinable())
    mThread.join();
}

void SidecarWriter::Write(const SidecarState &state) {
  {
    std::lock_guard lock(mMutex);
    mQueued[SidecarPath(mDirectory, state.Identity)] = state;
  }
  mWake.notify_one();
}

void SidecarWriter::WaitIdle() {
  std::unique_lock lock(mMutex);
  mIdle.wait(lock, [this] { return mQueued.empty() && !mWriting; });
}

std::optional<std::string> SidecarWriter::TakeError() {
  std::lock_guard lock(mMutex);
  return std::exchange(mError, std::nullopt);
}

void SidecarWriter::Run() {
  std::unique_lock lock(mMutex);
  for (;;) {
    mWake.wait(lock, [this] { return mStop || !mQueued.empty(); });
    if (mQueued.empty()) {
      if (mStop)
        return;
      continue;
    }
    auto queued = std::move(mQueued);
    mQueued.clear();
    mWriting = true;
    lock.unlock();
    std::optional<std::string> failure;
    for (const auto &[path, state] : queued) {
      std::string error;
      if (!SaveSidecar(mDirectory, state, &error))
        failure = error;
    }
    lock.lock();
    mWriting = false;
    if (failure.has_value())
      mError = failure;
    mIdle.notify_all();
  }
}

} // namespace Oot3dArchipelago
