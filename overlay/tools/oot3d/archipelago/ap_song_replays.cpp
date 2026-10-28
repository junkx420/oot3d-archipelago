#include "ap_song_replays.h"

#include "ap_guest_layout.h"

namespace Oot3dArchipelago {

const char *ToString(SongReplayMode mode) noexcept {
  switch (mode) {
  case SongReplayMode::Off:
    return "off";
  case SongReplayMode::Skip:
    return "skip";
  case SongReplayMode::SkipNoSound:
    return "skip_no_sound";
  }
  return "off";
}

std::optional<SongReplayMode> ParseSongReplayMode(std::string_view text) noexcept {
  for (const auto mode : {SongReplayMode::Off, SongReplayMode::Skip, SongReplayMode::SkipNoSound})
    if (text == ToString(mode))
      return mode;
  return std::nullopt;
}

bool SkipSongReplay(SongReplayMode mode, oot3d::recomp::a32::MemoryBus &memory) {
  std::uint32_t play = 0;
  std::uint8_t msgMode = 0;
  std::uint16_t song = 0, scene = 0;
  if (mode == SongReplayMode::Off || !memory.Read32(kGlobalContextPointer, &play) || play == 0U ||
      !memory.Read8(play + kPlayMsgMode, &msgMode) || !memory.Read16(play + kPlayLastPlayedSong, &song) ||
      !memory.Read16(play + kPlaySceneNum, &scene))
    return false;
  std::uint8_t next = msgMode;
  if ((mode == SongReplayMode::SkipNoSound && msgMode == kMsgModeReplayMusic && song != kSongScarecrow) ||
      msgMode == kMsgModeReplay)
    next = scene == kSceneWaterTemple && song == kSongZeldasLullaby ? kMsgModeReplay + 1U : kMsgModeReplayEnd;
  else if (msgMode > kMsgModeReplay && msgMode < kMsgModeReplayEnd)
    next = static_cast<std::uint8_t>(msgMode + 1U);
  return next != msgMode && memory.Write8(play + kPlayMsgMode, next);
}

} // namespace Oot3dArchipelago
