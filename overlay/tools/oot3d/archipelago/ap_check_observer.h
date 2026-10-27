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

#include "ap_guest_layout.h"
#include "ap_location_key.h"
#include "ap_quick_text.h"
#include "recomp/a32_runtime.h"

#include <array>
#include <cstdint>
#include <functional>
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

inline constexpr std::array<GuestFunction, 9> kGuestFunctions{{
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
  std::function<bool()> mReceiving;
  std::function<bool()> mForestExitOpen;
  // The latest token's offer: chests nearby offer every frame and would
  // replace it in mLastOffer.
  std::optional<LocationKey> mLastTokenKey;
  // questItems as it was before the guard's test: address and value.
  std::optional<std::pair<std::uint32_t, std::uint32_t>> mGuardRestore;
  std::function<void()> mDeathSink;
  BlueWarpSink mBlueWarpSink;
  std::optional<Offer> mLastOffer;
  std::optional<std::uint32_t> mProbePc;
  HookHits mHits{};
  std::uint64_t mSequence = 0;
  bool mVerified = false;
  QuickText mQuickText;
};

} // namespace Oot3dArchipelago
