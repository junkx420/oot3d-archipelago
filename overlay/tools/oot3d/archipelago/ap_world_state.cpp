#include "ap_world_state.h"

#include "ap_save_flags.h"

#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

// slot_data values: the option's number (Options.py).
constexpr int kJabuJabuOpen = 1;
constexpr int kZorasFountainOpen = 2;
constexpr int kRainbowBridgeAlwaysOpen = 1;
constexpr int kGanonsTrialsSkip = 0;

constexpr SaveFlag kJabuJabuFlag{"offered a fish to Jabu-Jabu", EventChkInf(0x3U), FlagWidth::U16, 0x0400U};
constexpr SaveFlag kKingZoraFlag{"King Zora moved aside", EventChkInf(0x3U), FlagWidth::U16, 0x0008U};
constexpr SaveFlag kRainbowBridgeFlag{"rainbow bridge built", EventChkInf(0x4U), FlagWidth::U16, 0x2000U};
constexpr SaveFlag kBarrierFlag{"Ganon's Tower barrier dispelled", EventChkInf(0xCU), FlagWidth::U16, 0x0008U};

struct Trial {
  const char *Name; // GanonsTrials in Enums.py
  SaveFlag Flag;
};
constexpr std::array<Trial, 6> kTrials{{
    {"Forest Trial", {"Forest Trial done", EventChkInf(0xBU), FlagWidth::U16, 0x0800U}},
    {"Fire Trial", {"Fire Trial done", EventChkInf(0xBU), FlagWidth::U16, 0x4000U}},
    {"Water Trial", {"Water Trial done", EventChkInf(0xBU), FlagWidth::U16, 0x1000U}},
    {"Shadow Trial", {"Shadow Trial done", EventChkInf(0xBU), FlagWidth::U16, 0x2000U}},
    {"Spirit Trial", {"Spirit Trial done", EventChkInf(0xAU), FlagWidth::U16, 0x2000U}},
    {"Light Trial", {"Light Trial done", EventChkInf(0xBU), FlagWidth::U16, 0x8000U}},
}};

int IntField(const nlohmann::json &data, const char *name, int fallback) {
  const auto field = data.find(name);
  return field != data.end() && field->is_number_integer() ? field->get<int>() : fallback;
}

} // namespace

WorldState ParseWorldState(const std::string &slotDataJson) {
  WorldState state;
  const auto data = nlohmann::json::parse(slotDataJson, nullptr, false);
  if (!data.is_object())
    return state;
  state.OpenJabuJabu = IntField(data, "jabu_jabu", 0) == kJabuJabuOpen;
  state.OpenZorasFountain = IntField(data, "zoras_fountain", 0) == kZorasFountainOpen;
  state.OpenRainbowBridge = IntField(data, "rainbow_bridge", 0) == kRainbowBridgeAlwaysOpen;
  const int trials = IntField(data, "ganons_trials", -1);
  const auto required = data.find("required_trials");
  if (trials == kGanonsTrialsSkip) {
    for (const Trial &trial : kTrials)
      state.SkippedTrials.emplace_back(trial.Name);
  } else if (trials >= 0 && required != data.end() && required->is_array()) {
    for (const Trial &trial : kTrials)
      if (std::none_of(required->begin(), required->end(), [&](const nlohmann::json &name) {
            return name.is_string() && name.get<std::string>() == trial.Name;
          }))
        state.SkippedTrials.emplace_back(trial.Name);
  }
  return state;
}

int ApplyWorldState(const WorldState &state, MemoryBus &memory) {
  int set = 0;
  if (state.OpenJabuJabu)
    set += SetSaveFlag(memory, kJabuJabuFlag) ? 1 : 0;
  if (state.OpenZorasFountain)
    set += SetSaveFlag(memory, kKingZoraFlag) ? 1 : 0;
  if (state.OpenRainbowBridge)
    set += SetSaveFlag(memory, kRainbowBridgeFlag) ? 1 : 0;
  for (const Trial &trial : kTrials)
    if (std::find(state.SkippedTrials.begin(), state.SkippedTrials.end(), trial.Name) !=
        state.SkippedTrials.end())
      set += SetSaveFlag(memory, trial.Flag) ? 1 : 0;
  if (state.SkippedTrials.size() == kTrials.size())
    set += SetSaveFlag(memory, kBarrierFlag) ? 1 : 0;
  return set;
}

} // namespace Oot3dArchipelago
