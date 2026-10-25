#pragma once

// F1 > Game > Archipelago: server, slot, password, connect, status, console.

#include "ap_session.h"
#include "fast/oot3d/graphics_settings_window.h"

#include <memory>

namespace Oot3dArchipelago {

std::shared_ptr<Fast::Oot3d::GraphicsSettingsPanelTab> CreatePanel(Session &session);

} // namespace Oot3dArchipelago
