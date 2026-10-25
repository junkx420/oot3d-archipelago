#include "ap_panel.h"

#include <imgui.h>

#include <array>
#include <cstring>

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
    ImGui::Text("Locations scouted: %zu", stats.LocationsScouted);
    if (stats.LocationTableSize == 0U)
      ImGui::TextDisabled("No location table: checks are not sent yet.");

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

} // namespace Oot3dArchipelago
