#pragma once

// Skip song replays (a client option, off by default), as OoT3D_Randomizer's
// Settings_SkipSongReplays (settings.c). After Link plays a song the game
// plays it back; message modes 18 to 23 run that replay, and mode 23 ends it.
// Once per frame in gameplay the client moves the mode on as the randomizer
// does: from 19 (or 18, the replay's music, when the sound is skipped too)
// straight to 23, and through 20 to 22 one per frame.
//
// The song effects (OoT3D_Randomizer songs_visual_effects.c) are left as they
// are: they play out by themselves. The Song of Time and warp blocks' wait is
// shortened in the check observer (SetSongReplays).

#include "recomp/a32_runtime.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace Oot3dArchipelago {

enum class SongReplayMode : std::uint8_t {
  Off,         // the game's own replays
  Skip,        // the replay's music plays, the replay is skipped
  SkipNoSound, // the music is skipped too
};
const char *ToString(SongReplayMode mode) noexcept;
std::optional<SongReplayMode> ParseSongReplayMode(std::string_view text) noexcept;

inline constexpr std::uint8_t kMsgModeReplayMusic = 18U;
inline constexpr std::uint8_t kMsgModeReplay = 19U;
inline constexpr std::uint8_t kMsgModeReplayEnd = 23U;
// The scarecrow's song spawns Pierre during its replay music.
inline constexpr std::uint16_t kSongScarecrow = 12U;
// Zelda's Lullaby in the Water Temple goes through modes 20 to 22, for the
// dimmed bottom screen at its switches.
inline constexpr std::uint16_t kSongZeldasLullaby = 8U;
inline constexpr std::uint16_t kSceneWaterTemple = 5U;

// Moves the message mode on; returns whether it changed it. Call once per
// frame in gameplay with a save loaded.
bool SkipSongReplay(SongReplayMode mode, oot3d::recomp::a32::MemoryBus &memory);

} // namespace Oot3dArchipelago
