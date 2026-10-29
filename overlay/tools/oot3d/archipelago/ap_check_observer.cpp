#include "ap_check_observer.h"

#include "ap_sha256.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace Oot3dArchipelago {
namespace {

using oot3d::recomp::a32::GuestState;
using oot3d::recomp::a32::MemoryBus;

// Every hooked pc but quick text's (HookPcs sorts them).
constexpr std::array<std::uint32_t, 32> kCheckHooks{
    kEnCowInit,          kEnCowDestroy,          kRutoBlueWarpDoneBlock, kAdultBlueWarpBlock,
    kGameOverStartBlock, kActorOfferGetItem,     kItemGive,              kKokiriGuardCheckBlock,
    kKokiriGuardStaysBlock, kKokiriGuardMovesBlock, kCowBottleCheckBlock,   kChildBlueWarpBlock,
    kEnGirlAInit,        kEnGirlADraw,           kShopBuyEventDefault,   kShopCanBuyResultBlocks[0],
    kShopCanBuyResultBlocks[1], kShopCanBuyResultBlocks[2], kShopCanBuyResultBlocks[3],
    kShopCanBuyResultBlocks[4], kFastChestsObtainabilityBlock, kTimeBlockTimerBlock, kWarpBlockTimerBlock,
    kHealthChangeBy,     kMessageEntryOneBlock,  kMessageEntryTwoBlock,
    kSheikIceCavernCheckBlock, kSheikIceCavernShownBlock, kSheikIceCavernHiddenBlock,
    kEnBoxInitSmallReturnBlock, kEnBoxInitBigReturnBlock, kEnBoxUpdate};

// Chests remembered for their light; a scene has few, and actors are freed
// without a hook.
constexpr std::size_t kChangedChestsKept = 64U;

// Actors whose offered locations are remembered for item text (a scene has
// only a few offering at a time).
constexpr std::size_t kOfferedKeysKept = 32U;

// Unreadable guest memory reads as zero: the observer must never fault the
// game, and a zero actor or scene simply forms no key.
std::uint8_t Read8(MemoryBus &memory, std::uint32_t address) {
  std::uint8_t value = 0;
  return address != 0U && memory.Read8(address, &value) ? value : 0U;
}

std::uint16_t Read16(MemoryBus &memory, std::uint32_t address) {
  std::uint16_t value = 0;
  return address != 0U && memory.Read16(address, &value) ? value : 0U;
}

std::uint32_t Read32(MemoryBus &memory, std::uint32_t address) {
  std::uint32_t value = 0;
  return address != 0U && memory.Read32(address, &value) ? value : 0U;
}

std::uint8_t AbsoluteGetItemId(std::uint32_t value) {
  const auto signedValue = static_cast<std::int8_t>(value & 0xFFU);
  return static_cast<std::uint8_t>(signedValue < 0 ? -signedValue : signedValue);
}

} // namespace

CheckObserver::~CheckObserver() {
  if (!mReportSink)
    return;
  char text[512];
  const auto &quick = mQuickText.Counts();
  std::snprintf(text, sizeof(text),
                "[archipelago] hook hits: verified=%d probe(0x%08X)=%llu "
                "Actor_OfferGetItem=%llu Item_Give=%llu GameOver=%llu BlueWarp=%llu ForestExit=%llu "
                "CowBottle=%llu ShopBuys=%llu FastChests=%llu SongBlocks=%llu Damage=%llu Messages=%llu IceCavernSheik=%llu SaveTests=%llu Chests=%llu "
                "quick_text(%s): instant=%llu control_codes=%llu turbo=%llu%s%s",
                mVerified ? 1 : 0, mProbePc.value_or(0U),
                static_cast<unsigned long long>(mHits.Probe),
                static_cast<unsigned long long>(mHits.ActorOfferGetItem),
                static_cast<unsigned long long>(mHits.ItemGive),
                static_cast<unsigned long long>(mHits.GameOver),
                static_cast<unsigned long long>(mHits.BlueWarp),
                static_cast<unsigned long long>(mHits.ForestExit),
                static_cast<unsigned long long>(mHits.CowBottle), static_cast<unsigned long long>(mHits.ShopBuys),
                static_cast<unsigned long long>(mHits.FastChests),
                static_cast<unsigned long long>(mHits.SongBlocks),
                static_cast<unsigned long long>(mHits.Damage),
                static_cast<unsigned long long>(mHits.Messages),
                static_cast<unsigned long long>(mHits.IceCavernSheik),
                static_cast<unsigned long long>(mHits.SaveTests),
                static_cast<unsigned long long>(mHits.Chests),
                ToString(mQuickText.Mode()),
                static_cast<unsigned long long>(quick.Instant),
                static_cast<unsigned long long>(quick.ControlCodes),
                static_cast<unsigned long long>(quick.TurboPresses),
                mQuickText.Disabled().empty() ? "" : " disabled: ", mQuickText.Disabled().c_str());
  mReportSink(text);
}

std::vector<std::uint32_t> CheckObserver::HookPcs(std::optional<std::uint32_t> probePc) {
  std::vector<std::uint32_t> pcs(kCheckHooks.begin(), kCheckHooks.end());
  const auto text = QuickText::HookPcs();
  pcs.insert(pcs.end(), text.begin(), text.end());
  const auto saveTests = SaveTestHookPcs();
  pcs.insert(pcs.end(), saveTests.begin(), saveTests.end());
  if (probePc.has_value())
    pcs.push_back(*probePc);
  std::sort(pcs.begin(), pcs.end());
  pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
  return pcs;
}

bool CheckObserver::VerifyAddresses(MemoryBus &memory, std::string *error) {
  for (const GuestFunction &function : kGuestFunctions) {
    std::array<std::uint8_t, 32> window{};
    for (std::size_t i = 0; i < window.size(); ++i) {
      if (!memory.Read8(function.Address + static_cast<std::uint32_t>(i), &window[i])) {
        if (error != nullptr) {
          char text[128];
          std::snprintf(text, sizeof(text), "Archipelago: %s at 0x%08X is not mapped",
                        function.Name, function.Address);
          *error = text;
        }
        return false;
      }
    }
    if (Sha256Hex(window) != function.WindowSha256) {
      if (error != nullptr) {
        char text[192];
        std::snprintf(text, sizeof(text),
                      "Archipelago: %s at 0x%08X does not match its signature; "
                      "the running code image is not the supported EUR image",
                      function.Name, function.Address);
        *error = text;
      }
      return false;
    }
  }
  return true;
}

void CheckObserver::ObserveBlockEntry(std::uint32_t pc, GuestState &state, MemoryBus &memory) {
  if (!mVerified) {
    std::string error;
    if (!VerifyAddresses(memory, &error))
      throw std::runtime_error(error);
    mVerified = true;
  }
  if (mProbePc.has_value() && pc == *mProbePc)
    ++mHits.Probe;
  ObserveSaveTests(pc, state, memory);
  switch (pc) {
  case kActorOfferGetItem:
    ++mHits.ActorOfferGetItem;
    OnOffer(state, memory);
    break;
  case kItemGive:
    ++mHits.ItemGive;
    OnItemGive(state, memory);
    break;
  case kGameOverStartBlock:
    ++mHits.GameOver;
    if (mDeathSink)
      mDeathSink();
    break;
  case kChildBlueWarpBlock:
  case kRutoBlueWarpDoneBlock:
  case kAdultBlueWarpBlock: {
    // The play state is in r7 at the child and Jabu-Jabu blocks, r8 at the
    // adult one.
    const std::uint32_t play = pc == kAdultBlueWarpBlock ? state.r[8] : state.r[7];
    if (pc != kRutoBlueWarpDoneBlock)
      ++mHits.BlueWarp;
    if (mBlueWarpSink)
      mBlueWarpSink(pc, static_cast<std::uint8_t>(Read16(memory, play + kPlaySceneNum)), memory);
    break;
  }
  case kEnCowInit: {
    // EnCow_rInit numbers the cows of a scene before the game's init runs
    // (a cow's tail is one too): 1, then 2; the grottos by the scene they
    // return to.
    const std::uint8_t scene = static_cast<std::uint8_t>(Read16(memory, state.r[1] + kPlaySceneNum));
    const std::uint8_t returnScene = Read8(memory, kSaveRespawnReturnData);
    std::uint8_t number = mCowCount == 0U ? 1U : 2U;
    if (scene == kSceneGrottos && returnScene == kSceneDeathMountainTrail)
      number = 1U;
    else if (scene == kSceneGrottos && returnScene == kSceneHyruleField)
      number = 2U;
    ++mCowCount;
    mCows[state.r[0]] = number;
    break;
  }
  case kEnCowDestroy:
    mCowCount = 0U;
    mCows.erase(state.r[0]);
    break;
  case kCowBottleCheckBlock:
    if (const auto number = CowNumber(state.r[4]); number.has_value() && mCowGives) {
      const LocationKey key{static_cast<std::uint8_t>(Read16(memory, state.r[5] + kPlaySceneNum)),
                            OverrideType::BaseItem, static_cast<std::uint8_t>(kGetItemMilkBottle + *number)};
      if (state.r[0] == 0U && mCowGives(key)) {
        state.r[0] = 1U; // "has an empty bottle"
        ++mHits.CowBottle;
      }
    }
    break;
  case kEnGirlAInit: {
    // ShopsanityItem_Init numbers the shelf in init order; a shelf starts
    // again when an actor comes back or eight are done.
    if (mShopSlots.count(state.r[0]) != 0U || mShopCount >= kShopSlotsPerShop) {
      mShopSlots.clear();
      mShopActive.clear();
      mShopCount = 0U;
    }
    std::uint32_t entrance = 0;
    (void)memory.Read32(kSaveContext + kSaveEntranceIndex, &entrance);
    const LocationKey key = ShopSlotKey(static_cast<std::uint8_t>(Read16(memory, state.r[1] + kPlaySceneNum)),
                                        mShopCount++, static_cast<std::int32_t>(entrance));
    mShopSlots[state.r[0]] = key;
    // A bought slot starts sold out.
    if (mShopSlot && mShopSlot(key, memory).Type == ShopSlotState::Kind::SoldOut)
      (void)memory.Write16(state.r[0] + kActorParams, kShopItemSoldOut);
    break;
  }
  case kEnGirlADraw: {
    const auto slot = mShopSlots.find(state.r[0]);
    if (slot == mShopSlots.end() || !mShopSlot)
      break;
    const ShopSlotState sells = mShopSlot(slot->second, memory);
    if (sells.Type == ShopSlotState::Kind::Sell && ApplyShopSlot(state.r[0], sells.Item, memory))
      mShopActive[state.r[0]] = sells.Item;
    else
      mShopActive.erase(state.r[0]);
    break;
  }
  case kShopBuyEventDefault: {
    const auto active = mShopActive.find(state.r[1]);
    const auto slot = mShopSlots.find(state.r[1]);
    if (active == mShopActive.end() || slot == mShopSlots.end())
      break;
    ++mHits.ShopBuys;
    if (mShopBought)
      mShopBought(slot->second);
    // Sold out from now on; the buy event goes on to take the price.
    (void)SellOutShopSlot(state.r[1], memory);
    mShopActive.erase(active);
    break;
  }
  case kShopCanBuyResultBlocks[0]:
  case kShopCanBuyResultBlocks[1]:
  case kShopCanBuyResultBlocks[2]:
  case kShopCanBuyResultBlocks[3]:
  case kShopCanBuyResultBlocks[4]:
    if (const auto active = mShopActive.find(state.r[6]); active != mShopActive.end()) {
      const auto rupees = static_cast<std::int16_t>(Read16(memory, kSaveContext + kSaveRupees));
      state.r[0] = rupees >= active->second.Price ? kCanBuySuccessFanfare : kCanBuyNeedRupees;
    }
    break;
  case kFastChestsObtainabilityBlock:
    // Not ITEM_NONE: the game's own short animation.
    if (mFastChests && state.r[0] == kItemNone) {
      state.r[0] = 0U;
      ++mHits.FastChests;
    }
    break;
  case kEnBoxInitSmallReturnBlock:
  case kEnBoxInitBigReturnBlock: {
    // r4: the chest, r5: the play state.
    const std::uint32_t chest = state.r[4];
    mChangedChests.erase(chest);
    if (mChestAppearance.load() == ChestAppearance::Vanilla || !mChestContents)
      break;
    const auto scene = static_cast<std::uint8_t>(Read16(memory, state.r[5] + kPlaySceneNum));
    const auto key = ChestKey(Read16(memory, chest + kActorParams), scene);
    const auto contents = key.has_value() ? mChestContents(*key) : std::nullopt;
    if (contents.has_value() && ApplyChestAppearance(chest, scene, *contents, memory)) {
      if (mChangedChests.size() >= kChangedChestsKept)
        mChangedChests.clear();
      mChangedChests.insert(chest);
      ++mHits.Chests;
    }
    break;
  }
  case kEnBoxUpdate:
    // r0: the chest. Its light (spawned as its child when it opens) gets the
    // chest's scale, as EnBox_rUpdate does after each update.
    if (mChangedChests.count(state.r[0]) != 0U) {
      const std::uint32_t light = Read32(memory, state.r[0] + kActorChild);
      for (std::uint32_t axis = 0; light != 0U && axis < 3U; ++axis)
        (void)memory.Write32(light + kActorScale + axis * 4U, Read32(memory, state.r[0] + kActorScale + axis * 4U));
    }
    break;
  case kTimeBlockTimerBlock:
  case kWarpBlockTimerBlock:
    // r0 is the block + 0x100. The timer the game set for the replay: start
    // it where the skipped replay leaves it.
    if (mSongReplays && Read16(memory, state.r[0] + kSongBlockTimer) == kSongBlockTimerReplay &&
        memory.Write16(state.r[0] + kSongBlockTimer, kSongBlockTimerSkipped))
      ++mHits.SongBlocks;
    break;
  case kHealthChangeBy:
    // r1: the health change (s16).
    if (const auto multiplier = mDamageMultiplier.load(); multiplier != DamageMultiplier::Normal) {
      const auto change = static_cast<std::int16_t>(state.r[1] & 0xFFFFU);
      const std::int32_t scaled =
          mDamage.Apply(multiplier, change, Read8(memory, kSaveContext + kSaveDoubleDefense) != 0U);
      if (scaled != change) {
        state.r[1] = static_cast<std::uint32_t>(scaled);
        ++mHits.Damage;
      }
    }
    break;
  case kMessageEntryOneBlock:
  case kMessageEntryTwoBlock:
    // r0: the entry Message_GetEntry found (0: none).
    if (mMessageOverride && state.r[0] != 0U) {
      const std::uint32_t object = (pc == kMessageEntryOneBlock ? state.r[5] : state.r[4]) + kMessageObjectOffset;
      const std::uint32_t player = Read32(memory, Read32(memory, kGlobalContextPointer) + kPlayPlayerActor);
      const auto offered = mOfferedKeys.find(Read32(memory, player + kPlayerInteractRangeActor));
      const std::optional<LocationKey> offer =
          offered != mOfferedKeys.end() ? std::optional<LocationKey>(offered->second) : std::nullopt;
      if (const auto entry = mMessageOverride(Read32(memory, state.r[0]), state.r[0],
                                              Read32(memory, object + kMessageObjectTextBase), offer, memory)) {
        state.r[0] = *entry;
        ++mHits.Messages;
      }
    }
    break;
  case kKokiriGuardCheckBlock:
    if (mForestExitOpen && mForestExitOpen()) {
      const std::uint32_t questItems = state.r[8] + kSaveQuestItems;
      const std::uint32_t emerald = Read32(memory, state.r[6] + 0x48U);
      std::uint32_t value = 0;
      if (questItems == kSaveContext + kSaveQuestItems && emerald != 0U && memory.Read32(questItems, &value) &&
          (value & emerald) == 0U && memory.Write32(questItems, value | emerald)) {
        mGuardRestore.emplace(questItems, value);
        ++mHits.ForestExit;
      }
    }
    break;
  case kKokiriGuardStaysBlock:
  case kKokiriGuardMovesBlock:
    if (mGuardRestore.has_value()) {
      (void)memory.Write32(mGuardRestore->first, mGuardRestore->second);
      mGuardRestore.reset();
    }
    break;
  case kSheikIceCavernCheckBlock:
    if (mSeedActive && mSeedActive()) {
      const std::uint32_t play = Read32(memory, kGlobalContextPointer);
      std::uint16_t equipment = 0;
      if (play != 0U && Read16(memory, play + kPlaySceneNum) == kSceneIceCavern &&
          memory.Read16(kSaveContext + kSaveEquipment, &equipment)) {
        const bool opened = (Read32(memory, play + kPlayChestFlags) & kIceCavernBootsChestFlag) != 0U;
        const auto shown = static_cast<std::uint16_t>(opened ? equipment | kEquipmentIronBoots
                                                             : equipment & ~kEquipmentIronBoots);
        if (shown != equipment && memory.Write16(kSaveContext + kSaveEquipment, shown)) {
          mSheikRestore = equipment;
          ++mHits.IceCavernSheik;
        }
      }
    }
    break;
  case kSheikIceCavernShownBlock:
  case kSheikIceCavernHiddenBlock:
    if (mSheikRestore.has_value()) {
      (void)memory.Write16(kSaveContext + kSaveEquipment, *mSheikRestore);
      mSheikRestore.reset();
    }
    break;
  default:
    mQuickText.ObserveBlockEntry(pc, state, memory);
    break;
  }
}

void CheckObserver::ShowInSave(std::uint32_t address, std::uint8_t size, std::uint32_t value, MemoryBus &memory) {
  std::uint32_t old = 0;
  bool read = false;
  if (size == 1U) {
    std::uint8_t byte = 0;
    read = memory.Read8(address, &byte);
    old = byte;
  } else if (size == 2U) {
    std::uint16_t half = 0;
    read = memory.Read16(address, &half);
    old = half;
  } else {
    read = memory.Read32(address, &old);
  }
  if (!read || old == value)
    return;
  const bool written = size == 1U   ? memory.Write8(address, static_cast<std::uint8_t>(value))
                       : size == 2U ? memory.Write16(address, static_cast<std::uint16_t>(value))
                                    : memory.Write32(address, value);
  if (written) {
    mSaveRestore = SaveRestore{address, size, old};
    ++mHits.SaveTests;
  }
}

// Save tests (ap_save_tests.h). A change lasts for the one block that makes
// the test: whatever hooked block runs next restores it, normally one after
// the test's branch.
void CheckObserver::ObserveSaveTests(std::uint32_t pc, GuestState &state, MemoryBus &memory) {
  if (mSaveRestore.has_value()) {
    const SaveRestore restore = *mSaveRestore;
    mSaveRestore.reset();
    if (restore.Size == 1U)
      (void)memory.Write8(restore.Address, static_cast<std::uint8_t>(restore.Value));
    else if (restore.Size == 2U)
      (void)memory.Write16(restore.Address, static_cast<std::uint16_t>(restore.Value));
    else
      (void)memory.Write32(restore.Address, restore.Value);
  }
  if (!mSeedActive || !mSeedActive())
    return;
  switch (pc) {
  case kGoronTextDispatchBlock:
    // Link the Goron's texts never see the Fire Medallion.
    if ((Read16(memory, state.r[4] + kActorParams) & 0x1FU) == kGoronLinkParams) {
      const std::uint32_t quest = Read32(memory, kSaveContext + kSaveQuestItems);
      ShowInSave(kSaveContext + kSaveQuestItems, 4U, quest & ~kQuestFireMedallion, memory);
    }
    return;
  case kAdultGalleryQuiverBlock:
    (void)memory.Write32(state.r[4] + kAdultGalleryReward, kGetItemQuiver40);
    return;
  case kArcheryQuiverBlock:
    state.r[4] = kGetItemQuiver40;
    return;
  case kArcheryQuiverTwoBlock:
    state.r[5] = kGetItemQuiver40;
    return;
  default:
    break;
  }
  for (const SaveTest &test : kSaveTests) {
    if (test.CheckBlock != pc)
      continue;
    bool owned = false;
    if (test.By == SaveTestBy::Location) {
      const auto taken = mLocationTaken ? mLocationTaken(test.Location) : std::nullopt;
      if (!taken.has_value())
        return;
      owned = *taken;
    } else {
      if (!mRewardsShuffled || !mRewardsShuffled())
        return;
      owned = test.By == SaveTestBy::DungeonBeaten &&
              (Read16(memory, kSaveContext + test.EventOffset) & test.EventMask) != 0U;
    }
    const std::uint32_t address = kSaveContext + test.Offset;
    const std::uint32_t value = test.Size == 1U   ? Read8(memory, address)
                                : test.Size == 2U ? Read16(memory, address)
                                                  : Read32(memory, address);
    ShowInSave(address, test.Size, SaveTestValue(test, value, owned), memory);
    return;
  }
}

// Actor_OfferGetItem(Actor *actor, PlayState *play, s32 getItemId, ...).
// The game offers every frame while the player is in range; the latest offer
// is remembered until an Item_Give shows which one was accepted.
void CheckObserver::OnOffer(GuestState &state, MemoryBus &memory) {
  const std::uint32_t actor = state.r[0];
  const std::uint32_t play = state.r[1];
  if (actor == 0U || play == 0U)
    return;
  Offer offer;
  offer.Actor = actor;
  offer.Source.ActorId = Read16(memory, actor + kActorId);
  offer.Source.ActorParams = Read16(memory, actor + kActorParams);
  offer.Source.Scene = static_cast<std::uint8_t>(Read16(memory, play + kPlaySceneNum));
  offer.Source.GetItemId = AbsoluteGetItemId(state.r[2]);
  // A cow offers milk; its location is keyed by its number.
  if (offer.Source.ActorId == kActorCow)
    if (const auto number = CowNumber(actor))
      offer.Source.GetItemId = static_cast<std::uint8_t>(kGetItemMilkBottle + *number);
  if (offer.Source.ActorId == kActorEnItem00)
    offer.Source.CollectibleFlag = Read16(memory, actor + kEnItem00CollectibleFlag);
  offer.Source.RespawnReturnScene = Read8(memory, kSaveRespawnReturnData);
  if (mOverride && offer.Source.GetItemId != 0U) {
    if (const auto key = KeyForOffer(offer.Source)) {
      if (mOfferedKeys.size() >= kOfferedKeysKept && mOfferedKeys.count(actor) == 0U)
        mOfferedKeys.clear();
      mOfferedKeys.insert_or_assign(actor, *key);
      if (const auto replacement = mOverride(*key, memory); replacement.has_value() && *replacement != 0U) {
        const auto value = static_cast<std::int32_t>(*replacement & 0xFFU);
        const bool negative = static_cast<std::int32_t>(state.r[2]) < 0;
        state.r[2] = static_cast<std::uint32_t>(negative ? -value : value);
        offer.OverrideGetItemId = static_cast<std::uint8_t>(value);
      }
    }
  }
  if (offer.Source.ActorId == kActorSkullToken)
    mLastTokenKey = KeyForOffer(offer.Source);
  mLastOffer = offer;
}

// Item_Give(PlayState *play, u8 item).
void CheckObserver::OnItemGive(GuestState &state, MemoryBus &memory) {
  const std::uint32_t play = state.r[0];
  CheckEvent event;
  event.Sequence = ++mSequence;
  event.ItemId = static_cast<std::uint8_t>(state.r[1] & 0xFFU);
  event.Scene = static_cast<std::uint8_t>(Read16(memory, play + kPlaySceneNum));
  const std::uint32_t player = Read32(memory, play + kPlayPlayerActor);
  event.PlayerGetItemId = AbsoluteGetItemId(Read8(memory, player + kPlayerGetItemId));
  const std::uint32_t interactActor = Read32(memory, player + kPlayerInteractRangeActor);

  // A collected token reaches Item_Give after the player has already cleared
  // getItemId and interactRangeActor (seen in play, 2026-10-25), so it
  // matches the latest offer by actor kind instead, and only a token's offer:
  // getItemId may still hold an earlier chest's item, which a nearby chest
  // offering the same item matched (seen in play, 2026-10-26).
  const bool token = event.ItemId == kItemSkullToken;
  const bool receiving = mReceiving && mReceiving();
  // The token's location (its own offer, kept apart from other offers) may
  // hold another item: the token then becomes what the session says.
  std::optional<LocationKey> tokenKey;
  if (token && !receiving) {
    tokenKey = mLastTokenKey;
    mLastTokenKey.reset();
    if (tokenKey.has_value() && mDirectOverride)
      if (const auto replacement = mDirectOverride(*tokenKey, memory)) {
        state.r[1] = *replacement;
        event.OverrideGetItemId = *replacement;
      }
  }
  const bool offerAccepted =
      mLastOffer.has_value() &&
      (token ? mLastOffer->Source.ActorId == kActorSkullToken
             : (interactActor == mLastOffer->Actor ||
                (event.PlayerGetItemId != 0U && (event.PlayerGetItemId == mLastOffer->Source.GetItemId ||
                                                 event.PlayerGetItemId == mLastOffer->OverrideGetItemId))));
  if (tokenKey.has_value()) {
    event.Source = "offer";
    event.ActorId = kActorSkullToken;
    event.Key = tokenKey;
    if (mLastOffer.has_value() && mLastOffer->Source.ActorId == kActorSkullToken)
      mLastOffer.reset();
  } else if (offerAccepted) {
    event.Source = "offer";
    event.ActorId = mLastOffer->Source.ActorId;
    event.ActorParams = mLastOffer->Source.ActorParams;
    event.OfferGetItemId = mLastOffer->Source.GetItemId;
    event.OverrideGetItemId = mLastOffer->OverrideGetItemId;
    event.Key = KeyForOffer(mLastOffer->Source);
    // One offer, one check: a later give without its own offer must not
    // inherit this one.
    mLastOffer.reset();
  } else if (receiving) {
    // The client's own get-item (a received item) reaches Item_Give without
    // an offer too; it is no location.
    event.Source = "received";
  } else if (const auto key = KeyForDirectGive(event.ItemId)) {
    event.Source = "direct";
    event.Key = key;
    if (mDirectOverride) {
      if (const auto replacement = mDirectOverride(*key, memory)) {
        state.r[1] = *replacement;
        event.OverrideGetItemId = *replacement;
      }
    }
  }
  if (mSink)
    mSink(event);
}

std::optional<std::uint8_t> CheckObserver::CowNumber(std::uint32_t actor) const {
  const auto cow = mCows.find(actor);
  return cow != mCows.end() ? std::optional<std::uint8_t>(cow->second) : std::nullopt;
}

std::string FormatEvent(const CheckEvent &event) {
  char text[256];
  std::snprintf(text, sizeof(text),
                "[archipelago] check #%llu source=%s scene=0x%02X item=0x%02X "
                "player_gi=0x%02X actor=0x%04X params=0x%04X offer_gi=0x%02X override_gi=0x%02X",
                static_cast<unsigned long long>(event.Sequence), event.Source, event.Scene,
                event.ItemId, event.PlayerGetItemId, event.ActorId, event.ActorParams,
                event.OfferGetItemId, event.OverrideGetItemId);
  std::string line = text;
  line += event.Key.has_value() ? " " + Describe(*event.Key) : " key=none";
  return line;
}

} // namespace Oot3dArchipelago
