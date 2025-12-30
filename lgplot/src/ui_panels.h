/*
 * UI Panels
 * ImGui panel drawing functions.
 */
#pragma once

#include "imgui.h"

namespace lgplot {

// Panel drawing functions
void draw_controls_panel();
void draw_calibration_panel();
void draw_snapshot_panel();
void draw_spectrum_chart();
void draw_console_panel();

// Layout
void setup_default_docking_layout(ImGuiID dockspace_id);

// Calibration persistence
void save_calibration();
void load_calibration();

// Snapshot functions
void freeze_frame();
void unfreeze_frame();
bool export_snapshot();

// Peak detection
void detect_peaks();

} // namespace lgplot
