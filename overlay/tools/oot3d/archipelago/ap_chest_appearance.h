#pragma once

// Chest size and model match the contents (a client option, off by default),
// as OoT3D_Randomizer's "Classic" chest appearance (chest.c,
// Chest_ChangeAppearance): a chest with a major item or a boss key is big,
// any other small; a chest with a key has the fancy model, any other the
// wooden one. The randomizer's other appearances need its own chest
// textures, which this client does not ship.
//
// The randomizer changes the chest after EnBox_Init; the check observer does
// the same in the two blocks where EnBox_Init returns (after its last call,
// r4 the chest, r5 the play state). Model_EnableMeshGroupByIndex and
// Model_DisableMeshGroupByIndex only set a byte of the model's mesh group
// list, so the client writes those bytes itself. At EnBox_Update's entry
// the chest's light (its child, spawned when it opens) gets the chest's
// scale, as EnBox_rUpdate does after the update.
//
// What counts as major: the randomizer uses its item table; the client uses
// Archipelago's classification (progression and traps are major, as the
// randomizer's ice trap is), and the item's name for this world's keys,
// hearts and tokens, which the randomizer never makes big.

#include "ap_location_key.h"
#include "recomp/a32_runtime.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace Oot3dArchipelago {

// EnBox_Init's two returns (after Actor_SetFocus) and EnBox_Update, EUR.
inline constexpr std::uint32_t kEnBoxInitSmallReturnBlock = 0x00189F68U;
inline constexpr std::uint32_t kEnBoxInitBigReturnBlock = 0x00189F8CU;
inline constexpr std::uint32_t kEnBoxUpdate = 0x001D5B70U;

enum class ChestAppearance : std::uint8_t { Vanilla, Classic };
const char *ToString(ChestAppearance appearance) noexcept;
std::optional<ChestAppearance> ParseChestAppearance(std::string_view text) noexcept;

enum class ChestContents : std::uint8_t { Minor, Major, SmallKey, BossKey };

// Archipelago's item flags (NetworkItem.flags).
inline constexpr unsigned kItemFlagProgression = 1U;
inline constexpr unsigned kItemFlagTrap = 4U;

// What a location's item makes of its chest: name is this world's item name,
// empty for another game's item.
ChestContents ClassifyChestContents(std::string_view name, unsigned flags) noexcept;

// The chest's key, as Actor_OfferGetItem forms it from the chest.
std::optional<LocationKey> ChestKey(std::uint16_t params, std::uint8_t scene);

// Changes the chest at actor (after EnBox_Init) to match contents: model,
// scale, and the randomizer's moves of chests that are out of reach at the
// other size. False when the chest's model is unreadable (nothing changed).
bool ApplyChestAppearance(std::uint32_t actor, std::uint8_t scene, ChestContents contents,
                          oot3d::recomp::a32::MemoryBus &memory);

} // namespace Oot3dArchipelago
