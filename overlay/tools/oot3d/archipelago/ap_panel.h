#pragma once

// F1 > Game > Archipelago: server, slot, password, connect, status, console.

#include "ap_session.h"
#include "fast/oot3d/graphics_settings_window.h"

#include <memory>

namespace Oot3dArchipelago {

std::shared_ptr<Fast::Oot3d::GraphicsSettingsPanelTab> CreatePanel(Session &session);

// The Archipelago badge in the bottom-right corner of the title and the file
// selection, with the connection state; nothing in gameplay. Call inside the
// frame's ImGui pass.
void DrawBadge(const Session &session);

} // namespace Oot3dArchipelago
