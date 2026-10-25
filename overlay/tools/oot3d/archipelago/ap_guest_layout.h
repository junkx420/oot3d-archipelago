#pragma once

// Guest addresses and structure offsets the Archipelago client reads and
// writes, for the EUR execution image 16a6b0aa...
//
// Addresses come from addresses.json (OoT3D_Randomizer oot.ld, EUR). Offsets
// are computed from OoT3D_Randomizer's z3D headers for the 3DS target by
// recon/guest_layout/guest_layout.ps1; every value here appears in its output
// (EUR and USA share these structures).

#include <cstdint>

namespace Oot3dArchipelago {

// gSaveContext.
inline constexpr std::uint32_t kSaveContext = 0x00587958U;
inline constexpr std::uint32_t kSaveRespawnReturnData = kSaveContext + 0x14F0U + 0x1CU + 0x13U;
inline constexpr std::uint32_t kSaveCutsceneIndex = 0x0008U;    // s32
inline constexpr std::uint32_t kSaveHealthCapacity = 0x0042U;   // s16
inline constexpr std::uint32_t kSaveHealth = 0x0044U;           // s16
inline constexpr std::uint32_t kSaveMagicLevel = 0x0046U;       // s8
inline constexpr std::uint32_t kSaveMagic = 0x0047U;            // s8
inline constexpr std::uint32_t kSaveMagicAcquired = 0x004EU;    // u8
inline constexpr std::uint32_t kSaveDoubleMagic = 0x0050U;      // u8
inline constexpr std::uint32_t kSaveDoubleDefense = 0x0051U;    // u8
inline constexpr std::uint32_t kSaveItems = 0x008CU;            // u8[24]
inline constexpr std::uint32_t kSaveAmmo = 0x00A6U;             // s8[15]
inline constexpr std::uint32_t kSaveEquipment = 0x00B6U;        // u16
inline constexpr std::uint32_t kSaveUpgrades = 0x00B8U;         // u32
inline constexpr std::uint32_t kSaveQuestItems = 0x00BCU;       // u32
inline constexpr std::uint32_t kSaveDungeonItems = 0x00C0U;     // u8[20]
inline constexpr std::uint32_t kSaveDungeonKeys = 0x00D4U;      // s8[19]
inline constexpr std::uint32_t kSaveGsTokens = 0x00E8U;         // s16
inline constexpr std::uint32_t kSaveSceneFlags = 0x00ECU;       // SaveSceneFlags[124]
inline constexpr std::uint32_t kSaveSceneIndex = 0x007EU;       // u16
inline constexpr std::uint32_t kSaveEventChkInf = 0x0EECU;      // u16[14]
inline constexpr std::uint32_t kSaveItemGetInf = 0x0F08U;       // u16[4]
inline constexpr std::uint32_t kSaveInfTable = 0x0F10U;         // u16[30]
inline constexpr std::uint32_t kSaveOtherNewEventFlags = 0x13D4U; // u8
inline constexpr std::uint32_t kSaveGameMode = 0x14E4U;         // s32, 0 in gameplay
inline constexpr std::uint32_t kSaveMagicMeterSize = 0x1584U;   // u16
inline constexpr std::uint32_t kSaveHealthAccumulator = 0x15B2U; // s16
inline constexpr std::uint32_t kSceneFlagsSize = 0x1CU;
inline constexpr std::uint32_t kSceneFlagsChest = 0x00U;
inline constexpr std::uint32_t kSceneFlagsSwitch = 0x04U;
inline constexpr std::uint32_t kSceneFlagsCollect = 0x0CU;
inline constexpr std::uint32_t kSaveSceneCount = 124U;

inline constexpr std::uint8_t kSlotOcarina = 0x07U;
inline constexpr std::uint8_t kSlotBombchu = 0x08U;
inline constexpr std::uint8_t kSlotHookshot = 0x09U;
inline constexpr std::uint8_t kSlotBottle1 = 0x12U;
inline constexpr std::uint8_t kSlotBottle4 = 0x15U;
inline constexpr std::uint8_t kItemLetterRuto = 0x1BU;
inline constexpr std::uint8_t kItemNone = 0xFFU;

// Ganon's battle after the tower collapse (OoT3D_Randomizer entrance.c).
inline constexpr std::uint16_t kSceneGanonBattle = 0x4FU;

// The play state (GlobalContext): gGlobalContext holds its address.
inline constexpr std::uint32_t kGlobalContextPointer = 0x005043E0U;
inline constexpr std::uint32_t kPlaySceneNum = 0x0104U;
inline constexpr std::uint32_t kPlayActorCtx = 0x208CU;
inline constexpr std::uint32_t kPlaySceneLoadFlag = 0x5C2DU;
// actorCtx + actorList (0x0C) + ACTORTYPE_PLAYER (2) * 8 + first (4)
inline constexpr std::uint32_t kPlayPlayerActor = kPlayActorCtx + 0x0CU + 2U * 8U + 4U;
// actorCtx.flags (0x19C): the current scene's chest and collectible flags.
inline constexpr std::uint32_t kPlayChestFlags = kPlayActorCtx + 0x19CU + 0x10U;
inline constexpr std::uint32_t kPlayCollectFlags = kPlayActorCtx + 0x19CU + 0x1CU;
// actorCtx.titleCtx (0x1C0): the area title card.
inline constexpr std::uint32_t kPlayTitleDuration = kPlayActorCtx + 0x1C0U + 0x10U;
inline constexpr std::uint32_t kPlayTitleDelay = kPlayActorCtx + 0x1C0U + 0x11U;
inline constexpr std::uint32_t kPlayTitleAlpha = kPlayActorCtx + 0x1C0U + 0x12U;

// Actor and Player.
inline constexpr std::uint32_t kActorSize = 0x1A4U;
inline constexpr std::uint32_t kActorId = 0x000U;
inline constexpr std::uint32_t kActorParams = 0x01CU;
inline constexpr std::uint32_t kActorParent = 0x124U;
inline constexpr std::uint32_t kActorUpdate = 0x13CU;
inline constexpr std::uint32_t kActorDraw = 0x140U;
inline constexpr std::uint32_t kActorBgCheckFlags = 0x090U;
inline constexpr std::uint32_t kEnItem00CollectibleFlag = 0x1A8U;
inline constexpr std::uint32_t kPlayerGetItemId = 0x12ACU;
inline constexpr std::uint32_t kPlayerInteractRangeActor = 0x12B0U;
inline constexpr std::uint32_t kPlayerStateFlags1 = 0x1710U;
inline constexpr std::uint32_t kPlayerStateFlags2 = 0x1714U;
inline constexpr std::uint16_t kBgCheckGround = 0x0001U;
inline constexpr std::uint32_t kPlayerState1Submerged = 0x08000000U;
// OoT3D_Randomizer ItemOverride_CheckPlayerReadiness: states in which Link
// cannot take an item (talking, cutscenes, climbing, holding, ...).
inline constexpr std::uint32_t kPlayerState1Busy = 0xF4AC2485U;
inline constexpr std::uint32_t kPlayerState2Busy = 0x000C0000U;

// A guest address no title region uses (TriAevum's own regions sit at
// 0x06100000); the client maps one page there for its item actor.
inline constexpr std::uint32_t kClientRegionBase = 0x06200000U;
inline constexpr std::uint32_t kClientRegionSize = 0x1000U;

} // namespace Oot3dArchipelago
