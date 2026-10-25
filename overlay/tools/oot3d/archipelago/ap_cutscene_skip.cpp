#include "ap_cutscene_skip.h"

#include "ap_guest_layout.h"

#include <array>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

enum class Width : std::uint8_t { U8, U16, U32 };

struct SkipFlag {
  const char *What;
  std::uint32_t Offset; // in gSaveContext
  Width Size;
  std::uint32_t Bits;
};

constexpr std::uint32_t InfTable(std::uint32_t index) { return kSaveInfTable + index * 2U; }
constexpr std::uint32_t EventChkInf(std::uint32_t index) { return kSaveEventChkInf + index * 2U; }
constexpr std::uint32_t SceneSwitch(std::uint32_t scene) {
  return kSaveSceneFlags + scene * kSceneFlagsSize + kSceneFlagsSwitch;
}

// OoT3D_Randomizer SaveFile_Init, "things to always set" and the Four Poes
// cutscene option, limited to cutscenes and conversations.
constexpr std::array<SkipFlag, 10> kSkipFlags{{
    {"greeted by Saria", InfTable(0x00U), Width::U16, 0x0001U},
    {"met Darunia in the Fire Temple", InfTable(0x11U), Width::U16, 0x0400U},
    {"talked to the Lake Hylia owl", InfTable(0x19U), Width::U16, 0x0020U},
    {"Nabooru battle intro", EventChkInf(0x3U), Width::U16, 0x0800U},
    {"boss battle intros", EventChkInf(0x7U), Width::U16, 0x01FFU},
    {"Dodongo's Cavern intro", EventChkInf(0xBU), Width::U16, 0x0001U},
    {"met Ruto in the Water Temple", SceneSwitch(0x05U), Width::U32, 0x00010000U},
    {"talked to the Desert Colossus owl", SceneSwitch(0x5CU), Width::U32, 0x80000000U},
    {"Club Moblin cutscene", kSaveOtherNewEventFlags, Width::U8, 0x01U},
    {"Forest Temple Poe sisters cutscene", SceneSwitch(0x03U), Width::U32, 0x08000000U},
}};

constexpr std::uint16_t kSceneLinksHouse = 0x34U;
constexpr std::uint32_t kIntroCutscene = 0xFFF1U;

bool SetBits(MemoryBus &memory, const SkipFlag &flag) {
  const std::uint32_t address = kSaveContext + flag.Offset;
  switch (flag.Size) {
  case Width::U8: {
    std::uint8_t value = 0;
    return memory.Read8(address, &value) && (value & flag.Bits) != flag.Bits &&
           memory.Write8(address, static_cast<std::uint8_t>(value | flag.Bits));
  }
  case Width::U16: {
    std::uint16_t value = 0;
    return memory.Read16(address, &value) && (value & flag.Bits) != flag.Bits &&
           memory.Write16(address, static_cast<std::uint16_t>(value | flag.Bits));
  }
  case Width::U32: {
    std::uint32_t value = 0;
    return memory.Read32(address, &value) && (value & flag.Bits) != flag.Bits &&
           memory.Write32(address, value | flag.Bits);
  }
  }
  return false;
}

} // namespace

int ApplyCutsceneSkips(const CutsceneSkipOptions &options, MemoryBus &memory) {
  int set = 0;
  if (options.Cutscenes)
    for (const SkipFlag &flag : kSkipFlags)
      set += SetBits(memory, flag) ? 1 : 0;
  if (options.Intro) {
    // A new file starts in Link's house with the intro cutscene pending; the
    // game clears the index once the scene has read it, so a file past the
    // intro never matches.
    std::uint32_t cutscene = 0;
    std::uint16_t scene = 0;
    if (memory.Read32(kSaveContext + kSaveCutsceneIndex, &cutscene) && cutscene == kIntroCutscene &&
        memory.Read16(kSaveContext + kSaveSceneIndex, &scene) && scene == kSceneLinksHouse &&
        memory.Write32(kSaveContext + kSaveCutsceneIndex, 0U))
      ++set;
  }
  return set;
}

} // namespace Oot3dArchipelago
