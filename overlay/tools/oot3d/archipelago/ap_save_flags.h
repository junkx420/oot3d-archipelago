#pragma once

// Flags in gSaveContext that the client sets, as OoT3D_Randomizer's
// SaveFile_Init does for a new save. Bits are only ever added.

#include "ap_guest_layout.h"
#include "recomp/a32_runtime.h"

#include <cstdint>

namespace Oot3dArchipelago {

enum class FlagWidth : std::uint8_t { U8, U16, U32 };

struct SaveFlag {
  const char *What;
  std::uint32_t Offset; // in gSaveContext
  FlagWidth Size;
  std::uint32_t Bits;
};

constexpr std::uint32_t InfTable(std::uint32_t index) { return kSaveInfTable + index * 2U; }
constexpr std::uint32_t EventChkInf(std::uint32_t index) { return kSaveEventChkInf + index * 2U; }
constexpr std::uint32_t SceneSwitch(std::uint32_t scene) {
  return kSaveSceneFlags + scene * kSceneFlagsSize + kSceneFlagsSwitch;
}
// The game's EventSet(flag): eventChkInf[flag >> 4] bit (flag & 15).
constexpr SaveFlag Event(const char *what, std::uint32_t flag) {
  return {what, EventChkInf(flag >> 4U), FlagWidth::U16, 1U << (flag & 0xFU)};
}

// Adds the flag's bits; true when it changed the save.
inline bool SetSaveFlag(oot3d::recomp::a32::MemoryBus &memory, const SaveFlag &flag) {
  const std::uint32_t address = kSaveContext + flag.Offset;
  switch (flag.Size) {
  case FlagWidth::U8: {
    std::uint8_t value = 0;
    return memory.Read8(address, &value) && (value & flag.Bits) != flag.Bits &&
           memory.Write8(address, static_cast<std::uint8_t>(value | flag.Bits));
  }
  case FlagWidth::U16: {
    std::uint16_t value = 0;
    return memory.Read16(address, &value) && (value & flag.Bits) != flag.Bits &&
           memory.Write16(address, static_cast<std::uint16_t>(value | flag.Bits));
  }
  case FlagWidth::U32: {
    std::uint32_t value = 0;
    return memory.Read32(address, &value) && (value & flag.Bits) != flag.Bits &&
           memory.Write32(address, value | flag.Bits);
  }
  }
  return false;
}

inline bool SaveFlagSet(oot3d::recomp::a32::MemoryBus &memory, const SaveFlag &flag) {
  std::uint32_t value = 0;
  const std::uint32_t address = kSaveContext + flag.Offset;
  switch (flag.Size) {
  case FlagWidth::U8: {
    std::uint8_t byte = 0;
    if (!memory.Read8(address, &byte))
      return false;
    value = byte;
    break;
  }
  case FlagWidth::U16: {
    std::uint16_t half = 0;
    if (!memory.Read16(address, &half))
      return false;
    value = half;
    break;
  }
  case FlagWidth::U32:
    if (!memory.Read32(address, &value))
      return false;
    break;
  }
  return (value & flag.Bits) == flag.Bits;
}

} // namespace Oot3dArchipelago
