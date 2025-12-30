/*
 * UI Panels Module
 * ImGui panel drawing functions.
 */
#pragma once

#include "imgui.h"

namespace lgplot {

// Draw the controls panel (connection, settings)
void draw_controls_panel();

// Draw the spectrum chart panel
void draw_spectrum_chart();

// Draw the console log panel
void draw_console_panel();

// Setup default docking layout
void setup_default_docking_layout(ImGuiID dockspace_id);

} // namespace lgplot
