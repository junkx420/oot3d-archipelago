#include "ap_world_state.h"

#include "ap_save_flags.h"

#include <algorithm>
#include <bit>
#include <array>
#include <nlohmann/json.hpp>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

// slot_data values: the option's number (Options.py).
constexpr int kJabuJabuOpen = 1;
constexpr int kZorasFountainOpen = 2;
constexpr int kRainbowBridgeAlwaysOpen = 1;
// stones 2, medallions 3, dungeon rewards 4, dungeons 5, tokens 6 (greg 7 has
// no item in this game).
constexpr int kRainbowBridgeStones = 2;
constexpr int kRainbowBridgeTokens = 6;
constexpr int kGanonsTrialsSkip = 0;
constexpr int kDoorOfTimeOpen = 2;
constexpr int kKakarikoGateOpen = 1;
constexpr int kClosedForestDekuOnly = 1;
constexpr int kClosedForestOff = 2;

constexpr SaveFlag kJabuJabuFlag{"offered a fish to Jabu-Jabu", EventChkInf(0x3U), FlagWidth::U16, 0x0400U};
constexpr SaveFlag kKingZoraFlag{"King Zora moved aside", EventChkInf(0x3U), FlagWidth::U16, 0x0008U};
constexpr SaveFlag kRainbowBridgeFlag{"rainbow bridge built", EventChkInf(0x4U), FlagWidth::U16, 0x2000U};
constexpr SaveFlag kDoorOfTimeFlag{"Door of Time open", EventChkInf(0x4U), FlagWidth::U16, 0x0800U};
constexpr SaveFlag kKakarikoGateFlag{"spoke to the gate guard about the mask shop", InfTable(0x7U), FlagWidth::U16, 0x0040U};
constexpr SaveFlag kMidoFlag{"Mido moved aside", EventChkInf(0x0U), FlagWidth::U16, 0x0010U};
constexpr SaveFlag kBarrierFlag{"Ganon's Tower barrier dispelled", EventChkInf(0xCU), FlagWidth::U16, 0x0008U};

// The boss events the blue warps set (ap_cutscene_skip.cpp), the dungeons
// of the bridge's dungeons condition.
constexpr std::array<std::uint32_t, 8> kDungeonEvents{0x07U, 0x25U, 0x37U, 0x48U, 0x49U, 0x4AU, 0x47U, 0x46U};
constexpr std::uint32_t kQuestMedallions = 0x0000003FU;
constexpr std::uint32_t kQuestStones = 0x001C0000U;

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
  state.OpenDoorOfTime = IntField(data, "door_of_time", 0) == kDoorOfTimeOpen;
  state.OpenKakarikoGate = IntField(data, "kakariko_gate", 0) == kKakarikoGateOpen;
  const int forest = IntField(data, "closed_forest", 0);
  state.MidoMoved = forest == kClosedForestOff;
  state.OpenForestExit = forest == kClosedForestDekuOnly || forest == kClosedForestOff;
  const int bridge = IntField(data, "rainbow_bridge", 0);
  if (bridge >= kRainbowBridgeStones && bridge <= kRainbowBridgeTokens) {
    static constexpr std::array<const char *, 5> kCounts{
        "rainbow_bridge_stones_required", "rainbow_bridge_medallions_required",
        "rainbow_bridge_dungeon_rewards_required", "rainbow_bridge_dungeons_required",
        "rainbow_bridge_skull_tokens_required"};
    state.BridgeCondition = static_cast<WorldState::Bridge>(bridge - kRainbowBridgeStones + 1);
    state.BridgeCount = IntField(data, kCounts[static_cast<std::size_t>(bridge - kRainbowBridgeStones)], 0);
  }
  if (const int hearts = IntField(data, "starting_hearts", 3); hearts != 3 && hearts >= 1 && hearts <= 20)
    state.StartingHearts = hearts;
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
  if (state.OpenDoorOfTime)
    set += SetSaveFlag(memory, kDoorOfTimeFlag) ? 1 : 0;
  if (state.OpenKakarikoGate)
    set += SetSaveFlag(memory, kKakarikoGateFlag) ? 1 : 0;
  if (state.MidoMoved)
    set += SetSaveFlag(memory, kMidoFlag) ? 1 : 0;
  if (state.BridgeCondition != WorldState::Bridge::Game &&
      BridgeProgress(state.BridgeCondition, memory) >= state.BridgeCount)
    set += SetSaveFlag(memory, kRainbowBridgeFlag) ? 1 : 0;
  for (const Trial &trial : kTrials)
    if (std::find(state.SkippedTrials.begin(), state.SkippedTrials.end(), trial.Name) !=
        state.SkippedTrials.end())
      set += SetSaveFlag(memory, trial.Flag) ? 1 : 0;
  if (state.SkippedTrials.size() == kTrials.size())
    set += SetSaveFlag(memory, kBarrierFlag) ? 1 : 0;
  return set;
}

int BridgeProgress(WorldState::Bridge condition, MemoryBus &memory) {
  std::uint32_t quest = 0;
  (void)memory.Read32(kSaveContext + kSaveQuestItems, &quest);
  switch (condition) {
  case WorldState::Bridge::Stones:
    return std::popcount(quest & kQuestStones);
  case WorldState::Bridge::Medallions:
    return std::popcount(quest & kQuestMedallions);
  case WorldState::Bridge::Rewards:
    return std::popcount(quest & (kQuestStones | kQuestMedallions));
  case WorldState::Bridge::Dungeons: {
    int done = 0;
    for (const std::uint32_t event : kDungeonEvents)
      done += SaveFlagSet(memory, Event("dungeon beaten", event)) ? 1 : 0;
    return done;
  }
  case WorldState::Bridge::Tokens: {
    std::uint16_t tokens = 0;
    (void)memory.Read16(kSaveContext + kSaveGsTokens, &tokens);
    return static_cast<std::int16_t>(tokens);
  }
  case WorldState::Bridge::Game:
    break;
  }
  return 0;
}

bool ApplyStartingHearts(const WorldState &state, MemoryBus &memory) {
  if (state.StartingHearts == 0)
    return false;
  const auto health = static_cast<std::uint16_t>(state.StartingHearts * 16);
  return memory.Write16(kSaveContext + kSaveHealthCapacity, health) && memory.Write16(kSaveContext + kSaveHealth, health);
}

} // namespace Oot3dArchipelago
