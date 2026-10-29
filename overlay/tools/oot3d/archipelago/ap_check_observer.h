#pragma once

// Observes item checks (Phase 1) and, with an override set, changes what a
// location gives (Phase 4).
//
// Hooks are guest function entries, so they are block starts of every
// execution mode (interpreter, compiled functions, whole-AOT) and need no
// title-module rebuild. The observer reads guest state; its one write is the
// get-item argument of Actor_OfferGetItem (r2, which block-entry callbacks
// may change) when the override names another item. It never writes memory
// itself (its blue warp sink may set save flags) and never raises an
// observable exit.

#include "ap_damage.h"
#include "ap_guest_layout.h"
#include "ap_location_key.h"
#include "ap_messages.h"
#include "ap_quick_text.h"
#include "ap_save_tests.h"
#include "ap_shops.h"
#include "recomp/a32_runtime.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Oot3dArchipelago {

// Guest functions the observer hooks, on the EUR execution image
// 16a6b0aa... Addresses and window hashes come from addresses.json
// (OoT3D_Randomizer oot.ld, EUR offsets); the tests keep both in sync.
struct GuestFunction {
  const char *Name;
  std::uint32_t Address;
  const char *WindowSha256; // first 32 bytes at Address
};

inline constexpr std::uint32_t kActorOfferGetItem = 0x003724DCU;
inline constexpr std::uint32_t kItemGive = 0x00376A78U;
// Not a function: the block holding OoT3D_Randomizer's GameOverStart patch
// site, which runs once when Link dies without a fairy (addresses.json kind
// hook_block). It starts after a call, so every execution mode enters it.
inline constexpr std::uint32_t kGameOverStartBlock = 0x00252420U;
// Not a function: OoT3D_Randomizer's ChildBlueWarpOverride site, the block
// where a child blue warp starts its scene change (after a branch, so every
// execution mode enters it). r7 holds the play state.
inline constexpr std::uint32_t kChildBlueWarpBlock = 0x003F5774U;
// The same for the adult dungeons (AdultBlueWarpOverride); r8 holds the play
// state.
inline constexpr std::uint32_t kAdultBlueWarpBlock = 0x001E3F50U;
// After Jabu-Jabu's warp has started its scene change (RutoBlueWarpOverride
// + 0x44, the target of the branch that ends it); r7 holds the play state.
inline constexpr std::uint32_t kRutoBlueWarpDoneBlock = 0x001A1944U;
// The Kokiri at the forest exit (EnKo, params 3): the block that tests the
// Kokiri Emerald (r8 + 0xBC questItems against the mask at r6 + 0x48), and
// the two blocks after it.
inline constexpr std::uint32_t kKokiriGuardCheckBlock = 0x003ACCF4U;
inline constexpr std::uint32_t kKokiriGuardStaysBlock = 0x003ACD04U;
inline constexpr std::uint32_t kKokiriGuardMovesBlock = 0x003ACD28U;
// Sheik in the Ice Cavern (EnXc, params 8): the unmodified game shows her
// once Link has the Iron Boots (save equipment, tested in the check block);
// OoT3D_Randomizer once the Ice Cavern's Iron Boots chest (chest flag 2) is
// open, which is what the logic expects when that chest holds another item.
// The check block, and the two blocks after its branch (shown, hidden).
inline constexpr std::uint32_t kSheikIceCavernCheckBlock = 0x002B2518U;
inline constexpr std::uint32_t kSheikIceCavernShownBlock = 0x002B254CU;
inline constexpr std::uint32_t kSheikIceCavernHiddenBlock = 0x002B25B0U;
inline constexpr std::uint16_t kEquipmentIronBoots = 0x2000U; // gEquipMasks[3][1] << gEquipShifts[3]
inline constexpr std::uint32_t kIceCavernBootsChestFlag = 1U << 2U;
inline constexpr std::uint16_t kSceneIceCavern = 0x09U;
// Cows (OoT3D_Randomizer cow.c): their init and destroy, to number them as
// the randomizer does, and the block that reads Inventory_HasEmptyBottle's
// result (r0; r4 the cow, r5 the play state).
inline constexpr std::uint32_t kEnCowInit = 0x00189FD4U;
inline constexpr std::uint32_t kEnCowDestroy = 0x0018A3E4U;
inline constexpr std::uint32_t kCowBottleCheckBlock = 0x003F0660U;
// Shops (ap_shops.h): the shelf item's init (r0 the item, r1 the play state)
// and draw (the same), and the blocks after the shopkeeper's calls through
// EnGirlA.canBuyFunc (r0 the result, r6 the item). The default buy event
// (r1 the item) is kShopBuyEventDefault.
inline constexpr std::uint32_t kEnGirlAInit = 0x001D7F20U;
inline constexpr std::uint32_t kEnGirlADraw = 0x00210188U;
inline constexpr std::array<std::uint32_t, 5> kShopCanBuyResultBlocks{
    0x0016CD94U, 0x00190DC4U, 0x00190FA0U, 0x00191358U, 0x00191394U};
// Fast chests (OoT3D_Randomizer chest.c, Chest_OverrideAnimation): the block
// after Link's chest opening calls Item_CheckObtainability (r0 its result).
// ITEM_NONE (a new item) picks the long animation, anything else the short
// one; the chest (its +0x38C, set by the same code) then also skips the
// light and the fanfare.
inline constexpr std::uint32_t kFastChestsObtainabilityBlock = 0x00354D24U;
// Skip song replays (OoT3D_Randomizer hooks.s, SkipSongReplays_TimeBlocksFix
// and _WarpBlocksFix): after the Song of Time, a Song of Time block and a warp
// block set their timer (+0x1C6) to 0x6E frames, the replay's length; the
// randomizer sets 0x10 when replays are skipped. These blocks count the timer
// down, r0 the block + 0x100.
inline constexpr std::uint32_t kTimeBlockTimerBlock = 0x00208010U;
inline constexpr std::uint32_t kWarpBlockTimerBlock = 0x00208074U;
inline constexpr std::uint32_t kSongBlockTimer = 0xC6U;
inline constexpr std::uint16_t kSongBlockTimerReplay = 0x6EU;
inline constexpr std::uint16_t kSongBlockTimerSkipped = 0x10U;

inline constexpr std::array<GuestFunction, 69> kGuestFunctions{{
    {"Actor_OfferGetItem", kActorOfferGetItem,
     "6c88ffb201ce9bba5920ffb4a4fca8247ae3859eab9e30dc8be729388888ecc4"},
    {"Item_Give", kItemGive,
     "e4f40f57614484da8e6cad178fc76bc97f1a26418815587c7c735c1be5c401c5"},
    {"GameOverStartBlock", kGameOverStartBlock,
     "6a643f94311062f2ce147592b043023f63114857eef0a2b5aacb4254f8db40cd"},
    {"ChildBlueWarpBlock", kChildBlueWarpBlock,
     "f99ffc6b35545504883b7964b0ada1a0032aeb6af75df5010414c94a487ad101"},
    {"AdultBlueWarpBlock", kAdultBlueWarpBlock,
     "1bcabfbc271fa0697a9675bcc66dcde6eea3d0b563373a2560f9dfaa914bae0a"},
    {"RutoBlueWarpDoneBlock", kRutoBlueWarpDoneBlock,
     "d0d6e3fe56a65f5c096d6ac2d05b7143060754472dbd2abde000f89c9a9c1daa"},
    {"KokiriGuardCheckBlock", kKokiriGuardCheckBlock,
     "e73746039b96870608cab2c8d3711a2f0dab3606eac05554d9977acd126166ca"},
    {"KokiriGuardStaysBlock", kKokiriGuardStaysBlock,
     "61fb640fcd37e9e39de6f9fc329aa848f5459535bd238d5d69059ea477e21ce5"},
    {"KokiriGuardMovesBlock", kKokiriGuardMovesBlock,
     "9148a8d4f0d91bf185b43544dd27dbb51dbf3e5e8466d9001f6ace2fb744a655"},
    {"EnCow_Init", kEnCowInit, "79ab92595c500cbe7c5a748bade68987e7bf6eafbb1e77131e319c2401024105"},
    {"EnCow_Destroy", kEnCowDestroy, "ac7af3c9ce4f6b1167c80cf6b4f567ae840df21f1761fdb6a9f8be3cc6011db7"},
    {"CowBottleCheckBlock", kCowBottleCheckBlock,
     "f1be82057a6099e2376195398f0336ebbb6761aaed2adb7dc27103a372d6fb73"},
    {"EnGirlA_Init", kEnGirlAInit, "5d5365b561ee77ce90a2ea9e7134a59eecfebd9809df87c0ea245935249851d1"},
    {"EnGirlA_Draw", kEnGirlADraw, "065cc472cfb2eb551cd0f80e5843546cd26fc2b7c8fe7a55583e269b360bf1e7"},
    {"ShopBuyEventDefault", kShopBuyEventDefault, "23d969c0c9d1175bd2c1991e212325f888f76bc9e655f549a038409e1685596f"},
    {"ShopCanBuyResultBlock0", kShopCanBuyResultBlocks[0],
     "eaf326d83aee065ca3e53d7fd7c8211985dff7f0e50cdac68409e60e7b6037d0"},
    {"ShopCanBuyResultBlock1", kShopCanBuyResultBlocks[1],
     "5b243d06bdd3b4be244dc4da323d25faff5ef581faef68e669fb68ee42a98350"},
    {"ShopCanBuyResultBlock2", kShopCanBuyResultBlocks[2],
     "e332f4947e8a5f061d96144f75cc97f800963f401448a63cac3117462e3f9ff4"},
    {"ShopCanBuyResultBlock3", kShopCanBuyResultBlocks[3],
     "2da9d44b72d811fde9a7250af9ad067bfbc24ea4d58ae7a0055b3e520a1e6a49"},
    {"ShopCanBuyResultBlock4", kShopCanBuyResultBlocks[4],
     "32f5a48b7b0e069c1ce5ff7592352a794c8025915386acd9ac1a73c959ad2940"},
    {"FastChestsObtainabilityBlock", kFastChestsObtainabilityBlock,
     "8e397069a6866186efaa9178fd242e848823be9929144d6a5d19b7519323446e"},
    {"TimeBlockTimerBlock", kTimeBlockTimerBlock,
     "da73369795a18c27cc74d82f8aaa26c2a4708514acaf44d8f47f3a5937eb35ec"},
    {"WarpBlockTimerBlock", kWarpBlockTimerBlock,
     "da73369795a18c27cc74d82f8aaa26c2a4708514acaf44d8f47f3a5937eb35ec"},
    {"Health_ChangeBy", kHealthChangeBy,
     "1bb26079cc1ae03f7cae51e48399a9fcddd619f93cdb64eea684d5a15311a951"},
    {"MessageEntryOneBlock", kMessageEntryOneBlock,
     "08dfd8c5ff1d8b7c6c9938679bfec952e7b71d1a37320de78c8c24d160011c6b"},
    {"MessageEntryTwoBlock", kMessageEntryTwoBlock,
     "aebc1a9376203c2b04778a3c5863e1cc0f8f4b80613ba70b33c8e3bc6a39d750"},
    {"SheikIceCavernCheckBlock", kSheikIceCavernCheckBlock,
     "049f34b9ae5602f7da50e7770a8cc28ab9f7bd47ddd7dc7d4225381dcb2bf811"},
    {"SheikIceCavernShownBlock", kSheikIceCavernShownBlock,
     "f76b72626f056d46abeb1adc051e6bc61fa3652f7477482241b9acd86937a0af"},
    {"SheikIceCavernHiddenBlock", kSheikIceCavernHiddenBlock,
     "5fd1d490a2b4222f5330aa52425c9e467ad51adb0ad8a2eb04244575b4ca83a5"},
    // Save tests (ap_save_tests.h).
    {"DaruniaStrengthCheckBlock", 0x001E4878U,
     "6e4f66dbc247b42883c39a8aa138c01fb4fddcccf6e8f8ddce664b1f5a932266"},
    {"DaruniaStrengthOwnedBlock", 0x001E48B0U,
     "e955b1ec6b59e474acb45f0971eb86cc0c2d0305d33abd3b7f751c1fa066848a"},
    {"DaruniaStrengthTextBlock", 0x001E48BCU,
     "c1a6d90e8ddefe4bab57cf3bc614bc96e7fe1d510a43c75145ceeb77c50709e4"},
    {"SunFireArrowsCheckBlock", 0x003F23D0U,
     "e5096886bc3a23f5cd60ee136b2fa3c4d58fc83fb7880fd788883770e3eef380"},
    {"SunFireArrowsMissingBlock", 0x003F2404U,
     "132f3addb66c5c924d4c84f6c421ed0fef6aba17ab87ecb7a106d7ba55fd320f"},
    {"SunFireArrowsOwnedBlock", 0x003F2424U,
     "c5dc4ad286c36fffe2a66d6bcf6cba7815e29c79428eb14545329dd491431c9f"},
    {"SunFireArrowsCheckTwoBlock", 0x003F2440U,
     "28b02b4e4d82d9afdf84022d046df8ef06363a11fde2e1863e554455de1e665a"},
    {"SunFireArrowsMissingTwoBlock", 0x003F2450U,
     "7ff58c1989d42528b4f5863f960a246fd0720ef5d0637c4db7661d7aadc23af5"},
    {"SunFireArrowsOwnedTwoBlock", 0x003F24CCU,
     "3ff4b2f67c6d324994b9377300ecd5aecf8c755ef0fefabb8f0a77ea94fe5fcf"},
    {"GoronLinkDispatchBlock", 0x00195380U,
     "f0fb79504a6886a8335bff6c7a10d264626f95d36734aeddc7e15000739e0765"},
    {"GoronLinkMedallionBlock", 0x00195454U,
     "904b8b5425da5b7e8c3d881b5cb8738b0582e0d147e6fbeff6fa2fa7ffd4a781"},
    {"GoronLinkTunicCheckBlock", 0x0019546CU,
     "7220de8c6063f8854c90db912d51f308e8712e2faa6cc3a6cc9147f75515a59d"},
    {"GoronLinkHasTunicBlock", 0x00195484U,
     "f21521b4abcfc8b4ae6993f832cd5f52fd7b13f7de54b32c894f1890c8690277"},
    {"GoronLinkNoTunicBlock", 0x0019549CU,
     "f5773445ea9316ddfefbe4f114ca566e7e562a7866bbf8545186d72f5f5fecbc"},
    {"KingZoraTunicCheckBlock", 0x001B6A54U,
     "75347ad119fc64fab512e0e79a6711e8ad875f8b592a8b6c44501d27e157205b"},
    {"KingZoraTunicTextBlock", 0x001B6844U,
     "6f4106094f7b7e8209d4aff211c0ee014a3da6ec4fe1b2131116a095384a2d38"},
    {"BiggoronSwordCheckBlock", 0x00194D90U,
     "fe67d1b4abc56d9d2dafe5ff0e1628f03c04cff511bb4ab0a45913edbdf20ee0"},
    {"BiggoronSwordGivesBlock", 0x00194DB0U,
     "fb67a12ed06f97996286e2eaa0864b8f01abb0b18097374dc2cfc4c49e336c10"},
    {"BiggoronSwordOtherBlock", 0x00195310U,
     "ec9885aef51f2cc5100ea9a4ffb97179a7aa1687f4ce0fe9dd6242003fb65113"},
    {"RollingGoronBagCheckBlock", 0x001953E4U,
     "86419a05f6bfed781029f1ee76f26c93264b5e54ec1a8e87b41bdc3a367b8d63"},
    {"RollingGoronBagOwnedBlock", 0x00195418U,
     "b0e205399e9537f1c9fa92f38bcf554164b3efb240fa9d56d141b8ae005a0c5b"},
    {"RollingGoronBagMissingBlock", 0x00195434U,
     "29428ca0621d7db0cad1b834128d5b1ea782ad32d3c362ea3d9f8939a43e6ae7"},
    {"AdultGalleryQuiverBlock", 0x0038AB88U,
     "dd594ec921f2efb8c363ec8580ad9f9f6106c5fb3d11c9578ae296168d2019a0"},
    {"ArcheryQuiverBlock", 0x003AFA10U,
     "116cc8bf326f737c1489e6a6a21a7bff6cec9184242b80cf13662bd913139c0c"},
    {"ArcheryQuiverTwoBlock", 0x003C2D5CU,
     "116cc8bf326f737c1489e6a6a21a7bff6cec9184242b80cf13662bd913139c0c"},
    {"ImpaLullabyCheckBlock", 0x0044FD6CU,
     "d3933f06731e0496dfc55010efd452737bdb73a065a69bd7fa1ca49f1b2d2e2e"},
    {"ImpaLullabyTeachBlock", 0x0044FD7CU,
     "c2500d03eca73951169d4897d33e3cabae4df1c7c4281674ab7ba6cbd4a94b9a"},
    {"ImpaLullabyKnownBlock", 0x0044FDB4U,
     "c4bf45053bfc7b400680396220ee8df00eba82cf753d8a1445baa08b9fdcec08"},
    {"MalonEponaCheckBlock", 0x0018C560U,
     "91cc95a3268694d1356e847ba1069ab26c3f396d03784d8d1f12f7370946b123"},
    {"MalonEponaKnownBlock", 0x0018C574U,
     "da75067bace262479beea7c70c96e81ccf941551da48f8a3b00de23ffbfeee22"},
    {"MalonEponaTeachBlock", 0x0018C594U,
     "334a7093629618b46a3362f9c86aec592d287425ee86333121da2d0270c1cd26"},
    {"MalonEponaCheckTwoBlock", 0x00195CECU,
     "68a5bc78b73d678819c4ba072c72dbe75d2d810fb52187ef39c3da6f8643bb7c"},
    {"MalonEponaKnownTwoBlock", 0x00195D04U,
     "c78bf56116138c12e7cb0e88ff31bf9e3ee87c428de17f45937b80344d0cf5a7"},
    {"MalonEponaTeachTwoBlock", 0x00195D0CU,
     "8cf52fbd18dbe6287c3da085c7545be3375747ea6e6995a916b457e4d2d32697"},
    {"SariaSongCheckBlock", 0x00168698U,
     "74cde2027f0e98d3440f762fe788f14867eb2deae3c4eb9f8bcfa03b5ec5ff92"},
    {"SariaSongTeachBlock", 0x001686ACU,
     "f8db442808409edcf5ae2d4e4c4cc732a55cc22e7682bcbf00ce3e54047d2acb"},
    {"SariaSongTextBlock", 0x001686F0U,
     "7db74467903138bcc3c74aa1f3833badd59e08daefdf9cd2f4afda4a26246a94"},
    {"TombSunsSongCheckBlock", 0x002888B4U,
     "387249a039e390ecf33a364979cffc46627e050855dc25098b808dcd95582d10"},
    {"TombSunsSongTeachBlock", 0x002888CCU,
     "85045741ca25ba48f798eceb72895f18cf2bde5838b54a86aa0ed35de9946118"},
    {"TombSunsSongKnownBlock", 0x002888F0U,
     "87a1d9626bed18592d0b4198d6f5433c961a9e88bc2e23719b697104f704098b"},
}};

struct CheckEvent {
  std::uint64_t Sequence = 0;
  std::uint8_t Scene = 0;
  std::uint8_t ItemId = 0;          // Item_Give argument
  std::uint8_t PlayerGetItemId = 0; // player->getItemId at Item_Give, absolute
  std::uint16_t ActorId = 0;        // source actor, 0 when none
  std::uint16_t ActorParams = 0;
  std::uint8_t OfferGetItemId = 0;  // the matched offer's get-item id
  std::uint8_t OverrideGetItemId = 0; // what the override gave instead, 0 for none
  std::optional<LocationKey> Key;
  const char *Source = "none"; // "offer", "direct", "received" or "none"
};

std::string FormatEvent(const CheckEvent &event);

class CheckObserver final {
public:
  // verifyAddresses=false is for host tests whose fake memory holds no code.
  explicit CheckObserver(bool verifyAddresses = true)
      : mVerified(!verifyAddresses), mQuickText(verifyAddresses) {}
  // With a report sink, the hook hit counts are reported once on destruction
  // (the end of the game session).
  ~CheckObserver();
  CheckObserver(const CheckObserver &) = delete;
  CheckObserver &operator=(const CheckObserver &) = delete;
  void SetReportSink(std::function<void(const std::string &)> sink) {
    mReportSink = std::move(sink);
  }

  // Sorted guest pcs to add to the block-entry hook list. A probe pc (the
  // hook experiment, diagnostics only) is counted but not interpreted; it is
  // not signature-checked, so it must be an entry of the running image.
  static std::vector<std::uint32_t> HookPcs(std::optional<std::uint32_t> probePc = {});
  void SetProbePc(std::optional<std::uint32_t> probePc) noexcept { mProbePc = probePc; }

  // Compares every hooked function's window hash with the running image.
  // Returns false with a message naming the first mismatch.
  static bool VerifyAddresses(oot3d::recomp::a32::MemoryBus &memory, std::string *error);

  void SetSink(std::function<void(const CheckEvent &)> sink) { mSink = std::move(sink); }
  // Called when Link dies without a fairy.
  void SetDeathSink(std::function<void()> sink) { mDeathSink = std::move(sink); }
  // Called at the blue warp blocks (kChildBlueWarpBlock, kAdultBlueWarpBlock,
  // kRutoBlueWarpDoneBlock) with the block and the scene being left; the sink
  // may set save flags (ap_cutscene_skip.h).
  using BlueWarpSink =
      std::function<void(std::uint32_t pc, std::uint8_t scene, oot3d::recomp::a32::MemoryBus &memory)>;
  void SetBlueWarpSink(BlueWarpSink sink) { mBlueWarpSink = std::move(sink); }
  // Called for every offer that forms a location key; a value replaces the
  // offered get-item id (its sign is kept).
  using GetItemOverride = std::function<std::optional<std::uint16_t>(const LocationKey &key,
                                                                     oot3d::recomp::a32::MemoryBus &memory)>;
  void SetOverride(GetItemOverride override) { mOverride = std::move(override); }
  // Called for a direct give (no offer) that forms a location key; a value
  // replaces the item Item_Give receives (r1), so the game keeps nothing of
  // its own where the location holds another item.
  using DirectGiveOverride =
      std::function<std::optional<std::uint8_t>(const LocationKey &key, oot3d::recomp::a32::MemoryBus &memory)>;
  void SetDirectGiveOverride(DirectGiveOverride override) { mDirectOverride = std::move(override); }
  // Whether the client is giving a received item right now: its Item_Give
  // has no offer either and must not form a key.
  void SetReceivingProbe(std::function<bool()> probe) { mReceiving = std::move(probe); }
  // Whether the forest exit is open (closed_forest deku_only or off): the
  // Kokiri there then sees the Kokiri Emerald for the one block that tests
  // it, and the save is restored in the block after (the observer's only
  // write of its own).
  void SetForestExitProbe(std::function<bool()> probe) { mForestExitOpen = std::move(probe); }
  // Whether a save of a connected seed is open: Sheik in the Ice Cavern then
  // appears for the Iron Boots chest instead of the Iron Boots, the save's
  // boots bit set as the chest flag for the one block that tests it.
  void SetSeedProbe(std::function<bool()> probe) { mSeedActive = std::move(probe); }
  // Whether a location of the connected seed was taken (nothing: no seed,
  // or not one of its locations), for the save tests (ap_save_tests.h).
  using LocationTakenProbe = std::function<std::optional<bool>(const LocationKey &key)>;
  void SetLocationTakenProbe(LocationTakenProbe probe) { mLocationTaken = std::move(probe); }
  // Whether a cow's location (its key: scene, base item, 0x14 + the cow's
  // number) still holds an item: the cow then gives without an empty bottle.
  void SetCowProbe(std::function<bool(const LocationKey &key)> probe) { mCowGives = std::move(probe); }
  // What a shop slot (its key, ap_shops.h) sells: the game's item, another
  // item, or nothing (sold out); and the sink told when a changed slot is
  // bought.
  struct ShopSlotState {
    enum class Kind : std::uint8_t { Game, Sell, SoldOut } Type = Kind::Game;
    ShopSlotItem Item{};
  };
  using ShopSlotProbe = std::function<ShopSlotState(const LocationKey &key, oot3d::recomp::a32::MemoryBus &memory)>;
  void SetShopProbe(ShopSlotProbe probe) { mShopSlot = std::move(probe); }
  void SetShopBoughtSink(std::function<void(const LocationKey &key)> sink) { mShopBought = std::move(sink); }
  // Fast chests: every chest opens with the short animation, as small ones
  // do (a client option, off by default).
  void SetFastChests(bool on) noexcept { mFastChests = on; }
  // Skip song replays: the Song of Time and warp blocks do not wait for the
  // skipped replay (the msgMode part runs in the session's frame update).
  void SetSongReplays(bool skip) noexcept { mSongReplays = skip; }
  // Damage multiplier: scales the damage Link takes (a client option).
  void SetDamageMultiplier(DamageMultiplier multiplier) noexcept { mDamageMultiplier = multiplier; }
  // Called for each message entry the game looks up (ap_messages.h): its
  // text id, the entry found, the text file's base and the location of the
  // actor Link interacts with, if it offered an item; a value is the
  // address of an entry to show instead.
  using MessageOverride = std::function<std::optional<std::uint32_t>(
      std::uint32_t textId, std::uint32_t entry, std::uint32_t textBase, const std::optional<LocationKey> &offer,
      oot3d::recomp::a32::MemoryBus &memory)>;
  void SetMessageOverride(MessageOverride override) { mMessageOverride = std::move(override); }
  // Called for every hooked block entry of the window, not only the
  // observer's own pcs: the first call runs VerifyAddresses (guest code is
  // mapped by then) and throws std::runtime_error on a mismatch.
  void ObserveBlockEntry(std::uint32_t pc, oot3d::recomp::a32::GuestState &state,
                         oot3d::recomp::a32::MemoryBus &memory);
  [[nodiscard]] bool Verified() const noexcept { return mVerified; }

  struct HookHits {
    std::uint64_t Probe = 0;
    std::uint64_t ActorOfferGetItem = 0;
    std::uint64_t ItemGive = 0;
    std::uint64_t GameOver = 0;
    std::uint64_t BlueWarp = 0;
    std::uint64_t ForestExit = 0;
    std::uint64_t CowBottle = 0;
    std::uint64_t ShopBuys = 0;
    std::uint64_t FastChests = 0;
    std::uint64_t SongBlocks = 0;
    std::uint64_t Damage = 0;
    std::uint64_t Messages = 0;
    std::uint64_t IceCavernSheik = 0;
    std::uint64_t SaveTests = 0;
  };
  [[nodiscard]] const HookHits &Hits() const noexcept { return mHits; }

  // Quick text rides on the same hooks (HookPcs includes its blocks).
  [[nodiscard]] QuickText &Text() noexcept { return mQuickText; }

private:
  struct Offer {
    std::uint32_t Actor = 0;
    OfferSource Source{};
    std::uint8_t OverrideGetItemId = 0;
  };
  void OnOffer(oot3d::recomp::a32::GuestState &state, oot3d::recomp::a32::MemoryBus &memory);
  void OnItemGive(oot3d::recomp::a32::GuestState &state, oot3d::recomp::a32::MemoryBus &memory);

  std::function<void(const CheckEvent &)> mSink;
  std::function<void(const std::string &)> mReportSink;
  GetItemOverride mOverride;
  DirectGiveOverride mDirectOverride;
  MessageOverride mMessageOverride;
  std::function<bool()> mReceiving;
  std::function<bool()> mForestExitOpen;
  // The latest token's offer: chests nearby offer every frame and would
  // replace it in mLastOffer.
  std::optional<LocationKey> mLastTokenKey;
  std::function<bool(const LocationKey &key)> mCowGives;
  ShopSlotProbe mShopSlot;
  std::function<void(const LocationKey &key)> mShopBought;
  // Shelf items by actor: their key (numbered as ShopsanityItem_Init numbers
  // them), and those whose fields the client changed.
  std::map<std::uint32_t, LocationKey> mShopSlots;
  std::uint32_t mShopCount = 0;
  std::map<std::uint32_t, ShopSlotItem> mShopActive;
  // The cows of the scene by actor, numbered 1 and 2 as EnCow_rInit does.
  std::map<std::uint32_t, std::uint8_t> mCows;
  std::uint32_t mCowCount = 0;
  std::optional<std::uint8_t> CowNumber(std::uint32_t actor) const;
  // questItems as it was before the guard's test: address and value.
  std::optional<std::pair<std::uint32_t, std::uint32_t>> mGuardRestore;
  std::function<bool()> mSeedActive;
  // The save's equipment as it was before Sheik's check block.
  std::optional<std::uint16_t> mSheikRestore;
  LocationTakenProbe mLocationTaken;
  // A save test's field as it was: restored at the next hooked block.
  struct SaveRestore {
    std::uint32_t Address = 0;
    std::uint8_t Size = 0;
    std::uint32_t Value = 0;
  };
  std::optional<SaveRestore> mSaveRestore;
  void ShowInSave(std::uint32_t address, std::uint8_t size, std::uint32_t value,
                  oot3d::recomp::a32::MemoryBus &memory);
  void ObserveSaveTests(std::uint32_t pc, oot3d::recomp::a32::GuestState &state,
                        oot3d::recomp::a32::MemoryBus &memory);
  std::function<void()> mDeathSink;
  BlueWarpSink mBlueWarpSink;
  std::optional<Offer> mLastOffer;
  // The location of each actor that offered lately, for the text of the one
  // Link takes from (his interactRangeActor) when several offer at once.
  std::map<std::uint32_t, LocationKey> mOfferedKeys;
  std::optional<std::uint32_t> mProbePc;
  HookHits mHits{};
  std::uint64_t mSequence = 0;
  bool mVerified = false;
  // Set from the options sink (the overlay's thread).
  std::atomic<bool> mFastChests{false};
  std::atomic<bool> mSongReplays{false};
  std::atomic<DamageMultiplier> mDamageMultiplier{DamageMultiplier::Normal};
  DamageScaler mDamage;
  QuickText mQuickText;
};

} // namespace Oot3dArchipelago
