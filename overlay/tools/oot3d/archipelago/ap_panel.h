#pragma once

// F1 > Game > Archipelago: server, slot, password, connect, status, console,
// client options; and the in-game overlay.

#include "ap_session.h"
#include "fast/oot3d/graphics_settings_window.h"

#include <filesystem>
#include <memory>

namespace Oot3dArchipelago {

std::shared_ptr<Fast::Oot3d::GraphicsSettingsPanelTab> CreatePanel(Session &session);
// The same tab while the module is off: a switch to turn it on at the next
// start (EnabledInSettings).
std::shared_ptr<Fast::Oot3d::GraphicsSettingsPanelTab> CreateDisabledPanel(
    std::filesystem::path settingsFile);

// Call inside the frame's ImGui pass. The Archipelago badge in the
// bottom-right corner of the title and the file selection, with the
// connection state, and the session's notifications at the top of the screen.
void DrawOverlay(const Session &session);

} // namespace Oot3dArchipelago
