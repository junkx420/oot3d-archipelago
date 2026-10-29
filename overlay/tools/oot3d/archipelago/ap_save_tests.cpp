#include "ap_save_tests.h"

namespace Oot3dArchipelago {
namespace {

constexpr auto MakeHookPcs() {
  std::array<std::uint32_t, kSaveTests.size() * 3U + 5U> pcs{};
  std::size_t count = 0;
  const auto add = [&](std::uint32_t pc) {
    if (pc == 0U)
      return;
    for (std::size_t i = 0; i < count; ++i)
      if (pcs[i] == pc)
        return;
    pcs[count++] = pc;
  };
  for (const auto &test : kSaveTests) {
    add(test.CheckBlock);
    for (const std::uint32_t pc : test.RestoreBlocks)
      add(pc);
  }
  add(kGoronTextDispatchBlock);
  add(kGoronLinkMedallionBlock);
  add(kAdultGalleryQuiverBlock);
  add(kArcheryQuiverBlock);
  add(kArcheryQuiverTwoBlock);
  return std::pair{pcs, count};
}

constexpr auto kHookPcs = MakeHookPcs();

} // namespace

std::span<const std::uint32_t> SaveTestHookPcs() noexcept { return {kHookPcs.first.data(), kHookPcs.second}; }

std::uint32_t SaveTestValue(const SaveTest &test, std::uint32_t value, bool taken) noexcept {
  if (test.OwnedUntilTaken && taken)
    return value;
  const bool owned = test.OwnedUntilTaken ? true : taken;
  if (!owned)
    return (value & ~test.Mask) | test.Missing;
  return (value & test.Mask) == test.Missing ? (value & ~test.Mask) | test.Owned : value;
}

} // namespace Oot3dArchipelago
