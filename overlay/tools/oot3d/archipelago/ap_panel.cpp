#include "ap_panel.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>

namespace Oot3dArchipelago {
namespace {

template <std::size_t N> void CopyText(std::array<char, N> &target, const std::string &text) {
  std::strncpy(target.data(), text.c_str(), N - 1U);
  target[N - 1U] = '\0';
}

class Panel final : public Fast::Oot3d::GraphicsSettingsPanelTab {
public:
  explicit Panel(Session &session) : mSession(session) {
    const auto settings = mSession.Settings();
    CopyText(mServer, settings.Server);
    CopyText(mSlot, settings.Slot);
  }

  const char *Label() const noexcept override { return "Archipelago"; }
  const char *Group() const noexcept override { return Fast::Oot3d::kGameSettingsGroup; }

  void Draw() override {
    const ConnectionState state = mSession.State();
    const bool idle = state == ConnectionState::Disconnected;
    ImGui::BeginDisabled(!idle);
    ImGui::InputText("Server", mServer.data(), mServer.size());
    ImGui::InputText("Slot name", mSlot.data(), mSlot.size());
    ImGui::InputText("Password", mPassword.data(), mPassword.size(),
                     ImGuiInputTextFlags_Password);
    ImGui::EndDisabled();
    if (idle) {
      if (ImGui::Button("Connect") && mSlot[0] != '\0')
        mSession.Connect({mServer.data(), mSlot.data(), mPassword.data()});
    } else if (ImGui::Button("Disconnect")) {
      mSession.Disconnect();
    }
    ImGui::SameLine();
    ImGui::Text("Status: %s", ToString(state));

    const auto stats = mSession.Stats();
    ImGui::Text("Save slot: %s", stats.SaveSlot >= 0 ? std::to_string(stats.SaveSlot + 1).c_str()
                                                     : "no file loaded");
    ImGui::Text("Checks sent: %zu   without location id: %zu", stats.ChecksReported,
                stats.ChecksUnmapped);
    ImGui::Text("Items from server: %zu   applied: %lld", stats.ItemsFromServer,
                static_cast<long long>(stats.ItemsApplied));
    ImGui::Text("Locations scouted: %zu   DeathLink: %s   Goal: %s", stats.LocationsScouted,
                stats.DeathLink ? "on" : "off", stats.GoalReached ? "reached" : "not yet");
    if (stats.LocationTableSize == 0U)
      ImGui::TextDisabled("No location table: checks are not sent yet.");

    ImGui::Separator();
    CutsceneSkipOptions skips = mSession.Skips();
    bool changed = ImGui::Checkbox("Skip cutscenes", &skips.Cutscenes);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Story and boss cutscenes and first conversations that give nothing.");
    changed |= ImGui::Checkbox("Skip the intro of a new file (experimental)", &skips.Intro);
    if (changed)
      mSession.SetSkips(skips);

    ImGui::Separator();
    if (ImGui::BeginChild("archipelago_console", ImVec2(0.0F, 220.0F), true)) {
      for (const auto &line : mSession.Console())
        ImGui::TextWrapped("%s", line.c_str());
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0F);
    }
    ImGui::EndChild();
    const bool send = ImGui::InputText("##say", mSay.data(), mSay.size(),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Send") || send) && mSay[0] != '\0') {
      mSession.Say(mSay.data());
      mSay[0] = '\0';
    }
  }

private:
  Session &mSession;
  std::array<char, 128> mServer{};
  std::array<char, 64> mSlot{};
  std::array<char, 64> mPassword{};
  std::array<char, 256> mSay{};
};

} // namespace

std::shared_ptr<Fast::Oot3d::GraphicsSettingsPanelTab> CreatePanel(Session &session) {
  return std::make_shared<Panel>(session);
}

void DrawBadge(const Session &session) {
  if (!session.Stats().Frontend)
    return;
  const ConnectionState state = session.State();
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImDrawList *draw = ImGui::GetForegroundDrawList(const_cast<ImGuiViewport *>(viewport));
  ImFont *font = ImGui::GetFont();
  const float scale = std::max(1.0F, viewport->Size.y / 720.0F);
  const float titleSize = ImGui::GetFontSize() * 2.2F * scale;
  const float detailSize = ImGui::GetFontSize() * 1.25F * scale;
  const char *title = "Archipelago";
  const std::string detail =
      state == ConnectionState::SlotConnected   ? "Connected as " + session.Settings().Slot
      : state == ConnectionState::Disconnected ? std::string("Not connected - F1 > Archipelago")
                                               : "Status: " + std::string(ToString(state));
  const ImVec2 titleExtent = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0F, title);
  const ImVec2 detailExtent = font->CalcTextSizeA(detailSize, FLT_MAX, 0.0F, detail.c_str());

  // A ring of six colored dots in front of the name, after the Archipelago
  // logo's colors.
  const float margin = 18.0F * scale;
  const float ring = titleExtent.y * 0.55F;
  const float dot = ring * 0.32F;
  const float padding = 10.0F * scale;
  const float textWidth = std::max(titleExtent.x, detailExtent.x);
  const ImVec2 max(viewport->Pos.x + viewport->Size.x - margin,
                   viewport->Pos.y + viewport->Size.y - margin);
  const ImVec2 min(max.x - (padding * 3.0F + ring * 2.0F + textWidth),
                   max.y - (padding * 2.0F + titleExtent.y + detailExtent.y));
  draw->AddRectFilled(min, max, IM_COL32(10, 14, 30, 190), 8.0F * scale);
  const ImVec2 center(min.x + padding + ring, min.y + padding + titleExtent.y * 0.5F);
  constexpr std::array<ImU32, 6> kColors{IM_COL32(201, 118, 130, 255), IM_COL32(117, 194, 117, 255),
                                         IM_COL32(202, 148, 194, 255), IM_COL32(217, 160, 125, 255),
                                         IM_COL32(118, 126, 189, 255), IM_COL32(238, 227, 145, 255)};
  for (std::size_t index = 0; index < kColors.size(); ++index) {
    const float angle = static_cast<float>(index) * 6.2831853F / 6.0F - 1.5707963F;
    draw->AddCircleFilled(ImVec2(center.x + std::cos(angle) * (ring - dot),
                                 center.y + std::sin(angle) * (ring - dot)),
                          dot, kColors[index]);
  }
  const float textX = min.x + padding * 2.0F + ring * 2.0F;
  draw->AddText(font, titleSize, ImVec2(textX, min.y + padding), IM_COL32(240, 240, 240, 255), title);
  const ImU32 detailColor = state == ConnectionState::SlotConnected ? IM_COL32(140, 220, 140, 255)
                                                                    : IM_COL32(190, 190, 190, 255);
  draw->AddText(font, detailSize, ImVec2(textX, min.y + padding + titleExtent.y), detailColor,
                detail.c_str());
}

} // namespace Oot3dArchipelago
