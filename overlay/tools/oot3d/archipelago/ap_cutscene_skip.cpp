#include "ap_cutscene_skip.h"

#include "ap_guest_layout.h"
#include "ap_save_flags.h"

#include <array>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::MemoryBus;

// OoT3D_Randomizer SaveFile_Init, "things to always set" and the Four Poes
// cutscene option, limited to cutscenes and conversations.
constexpr std::array<SaveFlag, 10> kSkipFlags{{
    {"greeted by Saria", InfTable(0x00U), FlagWidth::U16, 0x0001U},
    {"met Darunia in the Fire Temple", InfTable(0x11U), FlagWidth::U16, 0x0400U},
    {"talked to the Lake Hylia owl", InfTable(0x19U), FlagWidth::U16, 0x0020U},
    {"Nabooru battle intro", EventChkInf(0x3U), FlagWidth::U16, 0x0800U},
    {"boss battle intros", EventChkInf(0x7U), FlagWidth::U16, 0x01FFU},
    {"Dodongo's Cavern intro", EventChkInf(0xBU), FlagWidth::U16, 0x0001U},
    {"met Ruto in the Water Temple", SceneSwitch(0x05U), FlagWidth::U32, 0x00010000U},
    {"talked to the Desert Colossus owl", SceneSwitch(0x5CU), FlagWidth::U32, 0x80000000U},
    {"Club Moblin cutscene", kSaveOtherNewEventFlags, FlagWidth::U8, 0x01U},
    {"Forest Temple Poe sisters cutscene", SceneSwitch(0x03U), FlagWidth::U32, 0x08000000U},
}};

constexpr std::uint32_t kIntroCutscene = 0xFFF1U;

// Cutscene_OverrideDekuTree and Cutscene_OverrideDodongosCavern: the events
// the skipped cutscenes would have set. The reward itself is the location's
// item, which comes from the server.
constexpr std::array<SaveFlag, 3> kDekuTreeWarpFlags{{
    Event("Deku Tree dead", 0x07U),
    Event("Deku Tree cutscene seen", 0x09U),
    {"spoke to Mido after the Deku Tree's death", EventChkInf(0x1U), FlagWidth::U16, 0x1000U},
}};
constexpr SaveFlag kDodongosCavernWarpFlag = Event("King Dodongo defeated", 0x25U);

// Cutscene_OverrideForestTemple to Cutscene_OverrideShadowTemple, by boss
// scene from 0x14. The game's adult warp (0x001E3F50) checks the event for
// the Forest, Fire and Water Temples and the medallion for the Spirit and
// Shadow Temples; the randomizer's events decide here, and the medallion is
// set with them (the location's item, the same medallion, comes again).
struct AdultWarp {
  SaveFlag Done;  // the randomizer's event for the dungeon: decides the skip
  SaveFlag Extra; // what the game's warp code tests besides, or what the
                  // skipped cutscene would have set
};
constexpr SaveFlag kNoFlag{"", 0U, FlagWidth::U8, 0U};
constexpr std::array<AdultWarp, 5> kAdultWarps{{
    {Event("Phantom Ganon defeated", 0x48U), kNoFlag},
    {Event("Volvagia defeated", 0x49U),
     SaveFlag{"Death Mountain's cloud normal again", EventChkInf(0x2U), FlagWidth::U16, 0x8000U}},
    {Event("Morpha defeated", 0x4AU),
     SaveFlag{"Lake Hylia's water raised", EventChkInf(0x6U), FlagWidth::U16, 0x0200U}},
    {Event("Twinrova defeated", 0x47U),
     SaveFlag{"Spirit Medallion", kSaveQuestItems, FlagWidth::U32, 0x00000008U}},
    {Event("Bongo Bongo defeated", 0x46U),
     SaveFlag{"Shadow Medallion", kSaveQuestItems, FlagWidth::U32, 0x00000010U}},
}};
constexpr std::uint16_t kJabuJabuWarpCutscene = 0xFFF0U;

} // namespace

int ApplyCutsceneSkips(const CutsceneSkipOptions &options, MemoryBus &memory) {
  int set = 0;
  if (options.Cutscenes)
    for (const SaveFlag &flag : kSkipFlags)
      set += SetSaveFlag(memory, flag) ? 1 : 0;
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

std::optional<std::uint8_t> SkipBlueWarpCutscene(std::uint8_t scene, MemoryBus &memory) {
  switch (scene) {
  case kSceneDekuTreeBoss:
    if (SaveFlagSet(memory, kDekuTreeWarpFlags[0]))
      return std::nullopt;
    for (const SaveFlag &flag : kDekuTreeWarpFlags)
      (void)SetSaveFlag(memory, flag);
    return kDungeonDekuTree;
  case kSceneDodongosCavernBoss:
    if (!SetSaveFlag(memory, kDodongosCavernWarpFlag))
      return std::nullopt;
    return kDungeonDodongosCavern;
  default:
    break;
  }
  if (scene < kSceneForestTempleBoss || scene > kSceneShadowTempleBoss)
    return std::nullopt;
  const AdultWarp &warp = kAdultWarps[scene - kSceneForestTempleBoss];
  if (!SetSaveFlag(memory, warp.Done))
    return std::nullopt;
  if (warp.Extra.Bits != 0U)
    (void)SetSaveFlag(memory, warp.Extra);
  return BossSceneDungeon(scene);
}

bool ClearJabuJabuWarpCutscene(std::uint8_t scene, MemoryBus &memory) {
  std::uint16_t cutscene = 0;
  return scene == kSceneJabuJabuBoss && memory.Read16(kSaveContext + kSaveNextCutsceneIndex, &cutscene) &&
         cutscene == kJabuJabuWarpCutscene && memory.Write16(kSaveContext + kSaveNextCutsceneIndex, 0U);
}

} // namespace Oot3dArchipelago
