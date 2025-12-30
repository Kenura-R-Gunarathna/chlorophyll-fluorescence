/*
 * UI Panels Implementation
 * ImGui panel drawing functions.
 */
#include "ui_panels.h"
#include "app_state.h"
#include "connection.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"

namespace lgplot {

void draw_controls_panel() {
    ImGui::Begin("Controls");
    
    // Connection status
    ImGui::Text("Status:");
    ImGui::SameLine();
    if (g_app.connection_mode != ConnectionMode::None) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), 
            g_app.connection_mode == ConnectionMode::USB ? "USB Connected" : "UDP Connected");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Disconnected");
    }
    
    ImGui::Separator();
    ImGui::Text("Connection");
    
    if (g_app.connection_mode == ConnectionMode::None) {
        // USB Serial connection
        ImGui::SetNextItemWidth(80);
        ImGui::InputText("##comport", g_app.com_port, sizeof(g_app.com_port));
        ImGui::SameLine();
        if (ImGui::Button("USB Connect", ImVec2(-1, 0))) {
            start_usb_receiver();
        }
        
        // UDP connection
        if (ImGui::Button("UDP Connect (WiFi)", ImVec2(-1, 0))) {
            start_udp_receiver();
        }
    } else {
        if (ImGui::Button("Disconnect", ImVec2(-1, 0))) {
            stop_receiver();
        }
    }
    
    ImGui::Separator();
    ImGui::Text("Statistics");
    ImGui::Text("Packets: %u", g_app.packets_received.load());
    ImGui::Text("Sequence: %u", g_app.last_sequence.load());
    ImGui::Text("Rate: %.1f pkt/s", g_app.packets_per_second.load());
    
    ImGui::Separator();
    ImGui::Text("Chart Settings");
    ImGui::Checkbox("Auto-fit Y Axis", &g_app.auto_fit_y);
    
    if (!g_app.auto_fit_y) {
        ImGui::SliderFloat("Y Min", &g_app.y_min, 0.0f, 4095.0f);
        ImGui::SliderFloat("Y Max", &g_app.y_max, 0.0f, 4095.0f);
    }
    
    ImGui::Checkbox("Show Grid", &g_app.show_grid);
    
    ImGui::Separator();
    if (ImGui::Checkbox("Dark Theme", &g_app.dark_theme)) {
        if (g_app.dark_theme) {
            ImGui::StyleColorsDark();
            ImPlot::StyleColorsDark();
        } else {
            ImGui::StyleColorsLight();
            ImPlot::StyleColorsLight();
        }
    }
    
    ImGui::End();
}

void draw_spectrum_chart() {
    ImGui::Begin("Spectrum");
    
    if (g_app.connection_mode != ConnectionMode::None && g_app.packets_per_second > 0) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "● LIVE");
        ImGui::SameLine();
    }
    ImGui::Text("TCD1304 CCD Spectrum (%d pixels)", CCD_PIXEL_COUNT);
    
    ImVec2 plot_size = ImVec2(-1, -1);
    
    if (ImPlot::BeginPlot("##Spectrum", plot_size)) {
        ImPlot::SetupAxes("Pixel Index", "Intensity (12-bit)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, CCD_PIXEL_COUNT, ImPlotCond_Once);
        
        if (g_app.auto_fit_y) {
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 4095, ImPlotCond_Once);
        } else {
            ImPlot::SetupAxisLimits(ImAxis_Y1, g_app.y_min, g_app.y_max, ImPlotCond_Always);
        }
        
        {
            std::lock_guard<std::mutex> lock(g_app.data_mutex);
            // Use stride of 2 to plot every 2nd point (halves rendering load)
            const int stride = 2;
            ImPlot::PlotLine("Intensity", g_app.pixel_indices.data(), g_app.spectrum_data.data(), 
                            CCD_PIXEL_COUNT / stride, 0, 0, sizeof(float) * stride);
        }
        
        ImPlot::EndPlot();
    }
    
    ImGui::End();
}

void draw_console_panel() {
    ImGui::Begin("Console");
    
    if (ImGui::Button("Clear")) {
        std::lock_guard<std::mutex> lock(g_app.log_mutex);
        g_app.console_log.clear();
    }
    ImGui::SameLine();
    ImGui::Text("Log (%zu lines)", g_app.console_log.size());
    
    ImGui::Separator();
    
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    {
        std::lock_guard<std::mutex> lock(g_app.log_mutex);
        for (const auto& line : g_app.console_log) {
            if (line.find("ERROR") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", line.c_str());
            } else if (line.find("WARNING") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", line.c_str());
            } else {
                ImGui::TextUnformatted(line.c_str());
            }
        }
    }
    
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
    ImGui::End();
}

void setup_default_docking_layout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);
    
    ImGuiID dock_left, dock_main;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.20f, &dock_left, &dock_main);
    
    ImGuiID dock_center, dock_bottom;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_center);
    
    ImGui::DockBuilderDockWindow("Controls", dock_left);
    ImGui::DockBuilderDockWindow("Spectrum", dock_center);
    ImGui::DockBuilderDockWindow("Console", dock_bottom);
    
    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace lgplot
