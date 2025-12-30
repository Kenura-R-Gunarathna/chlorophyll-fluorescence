/*
 * UI Panels Implementation
 * ImGui panel drawing functions with calibration, peak detection, and export.
 */
#include "ui_panels.h"
#include "app_state.h"
#include "connection.h"
#include "console.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lgplot {

// ============================================
// CALIBRATION FILE I/O
// ============================================
static const char* CALIBRATION_FILE = "calibration.ini";

void save_calibration() {
    std::ofstream file(CALIBRATION_FILE);
    if (file.is_open()) {
        file << "[Calibration]\n";
        file << "point1_pixel=" << g_app.cal_point1.pixel_index << "\n";
        file << "point1_wavelength=" << g_app.cal_point1.wavelength_nm << "\n";
        file << "point2_pixel=" << g_app.cal_point2.pixel_index << "\n";
        file << "point2_wavelength=" << g_app.cal_point2.wavelength_nm << "\n";
        file << "cal_A=" << g_app.cal_A << "\n";
        file << "cal_B=" << g_app.cal_B << "\n";
        file << "is_calibrated=" << (g_app.is_calibrated ? 1 : 0) << "\n";
        file.close();
        log_message("Calibration saved to %s", CALIBRATION_FILE);
    }
}

void load_calibration() {
    std::ifstream file(CALIBRATION_FILE);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("point1_pixel=") == 0)
                g_app.cal_point1.pixel_index = std::stoi(line.substr(13));
            else if (line.find("point1_wavelength=") == 0)
                g_app.cal_point1.wavelength_nm = std::stof(line.substr(18));
            else if (line.find("point2_pixel=") == 0)
                g_app.cal_point2.pixel_index = std::stoi(line.substr(13));
            else if (line.find("point2_wavelength=") == 0)
                g_app.cal_point2.wavelength_nm = std::stof(line.substr(18));
            else if (line.find("cal_A=") == 0)
                g_app.cal_A = std::stof(line.substr(6));
            else if (line.find("cal_B=") == 0)
                g_app.cal_B = std::stof(line.substr(6));
            else if (line.find("is_calibrated=") == 0)
                g_app.is_calibrated = (line.substr(14) == "1");
        }
        file.close();
        log_message("Calibration loaded from %s", CALIBRATION_FILE);
    }
}

// ============================================
// PEAK DETECTION
// ============================================
void detect_peaks() {
    g_app.detected_peaks.clear();
    
    const std::vector<float>& data = g_app.is_frozen ? g_app.snapshot_data : g_app.spectrum_data;
    
    for (int i = g_app.peak_min_distance; i < CCD_PIXEL_COUNT - g_app.peak_min_distance; i++) {
        float val = data[i];
        if (val < g_app.peak_threshold) continue;
        
        // Check if local maximum
        bool is_peak = true;
        for (int j = i - g_app.peak_min_distance; j <= i + g_app.peak_min_distance; j++) {
            if (j != i && data[j] >= val) {
                is_peak = false;
                break;
            }
        }
        
        if (is_peak) {
            Peak peak;
            peak.pixel_index = i;
            peak.intensity = val;
            peak.wavelength = g_app.wavelength_from_pixel(i);
            g_app.detected_peaks.push_back(peak);
        }
    }
}

// ============================================
// SNAPSHOT & EXPORT
// ============================================
static std::string get_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &time);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

void freeze_frame() {
    std::lock_guard<std::mutex> lock(g_app.data_mutex);
    g_app.snapshot_data = g_app.spectrum_data;
    g_app.is_frozen = true;
    detect_peaks();
    log_message("Frame frozen - %zu peaks detected", g_app.detected_peaks.size());
}

void unfreeze_frame() {
    g_app.is_frozen = false;
    log_message("Live mode resumed");
}

bool export_snapshot() {
    std::string timestamp = get_timestamp_string();
    std::string folder = std::string(g_app.export_folder) + "/" + timestamp;
    
    // Create folder
    std::filesystem::create_directories(folder);
    
    // Export CSV
    std::string csv_path = folder + "/spectrum_data.csv";
    std::ofstream csv(csv_path);
    if (csv.is_open()) {
        csv << "Pixel Index,Wavelength (nm),Intensity\n";
        for (int i = 0; i < CCD_PIXEL_COUNT; i++) {
            float wavelength = g_app.wavelength_from_pixel(i);
            csv << i << "," << wavelength << "," << g_app.snapshot_data[i] << "\n";
        }
        csv.close();
    }
    
    // Export peaks CSV
    std::string peaks_path = folder + "/peaks.csv";
    std::ofstream peaks_csv(peaks_path);
    if (peaks_csv.is_open()) {
        peaks_csv << "Peak #,Pixel Index,Wavelength (nm),Intensity\n";
        for (size_t i = 0; i < g_app.detected_peaks.size(); i++) {
            const auto& p = g_app.detected_peaks[i];
            peaks_csv << (i+1) << "," << p.pixel_index << "," << p.wavelength << "," << p.intensity << "\n";
        }
        peaks_csv.close();
    }
    
    // Export metadata
    std::string meta_path = folder + "/metadata.txt";
    std::ofstream meta(meta_path);
    if (meta.is_open()) {
        meta << "TCD1304 CCD Spectrum Snapshot\n";
        meta << "=============================\n";
        meta << "Timestamp: " << timestamp << "\n";
        meta << "Pixels: " << CCD_PIXEL_COUNT << "\n";
        meta << "Calibrated: " << (g_app.is_calibrated ? "Yes" : "No") << "\n";
        if (g_app.is_calibrated) {
            meta << "Wavelength formula: " << g_app.cal_A << " * pixel + " << g_app.cal_B << "\n";
            meta << "Wavelength range: " << g_app.wavelength_from_pixel(0) << " - " 
                 << g_app.wavelength_from_pixel(CCD_PIXEL_COUNT-1) << " nm\n";
        }
        meta << "Peaks detected: " << g_app.detected_peaks.size() << "\n";
        meta.close();
    }
    
    log_message("Snapshot saved to: %s", folder.c_str());
    return true;
}

// ============================================
// UI PANELS
// ============================================
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
        ImGui::SetNextItemWidth(80);
        ImGui::InputText("##comport", g_app.com_port, sizeof(g_app.com_port));
        ImGui::SameLine();
        if (ImGui::Button("USB Connect", ImVec2(-1, 0))) {
            start_usb_receiver();
        }
        
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
    ImGui::Checkbox("Show Wavelength Axis", &g_app.show_wavelength_axis);
    ImGui::Checkbox("Show Peaks", &g_app.show_peaks);
    
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

void draw_calibration_panel() {
    ImGui::Begin("Calibration");
    
    ImGui::Text("Wavelength Calibration");
    ImGui::TextWrapped("Enter two known wavelengths and their pixel positions.");
    
    ImGui::Separator();
    
    // Calibration point 1
    ImGui::Text("Point 1:");
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Pixel 1", &g_app.cal_point1.pixel_index);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputFloat("nm 1", &g_app.cal_point1.wavelength_nm, 0.0f, 0.0f, "%.1f");
    
    // Calibration point 2
    ImGui::Text("Point 2:");
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Pixel 2", &g_app.cal_point2.pixel_index);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputFloat("nm 2", &g_app.cal_point2.wavelength_nm, 0.0f, 0.0f, "%.1f");
    
    ImGui::Separator();
    
    if (ImGui::Button("Apply Calibration", ImVec2(-1, 0))) {
        g_app.calibrate();
        if (g_app.is_calibrated) {
            log_message("Calibration applied: wavelength = %.4f * pixel + %.2f", g_app.cal_A, g_app.cal_B);
            log_message("Range: %.1f - %.1f nm", 
                g_app.wavelength_from_pixel(0), 
                g_app.wavelength_from_pixel(CCD_PIXEL_COUNT-1));
        }
    }
    
    if (ImGui::Button("Save Calibration", ImVec2(-1, 0))) {
        save_calibration();
    }
    
    if (ImGui::Button("Load Calibration", ImVec2(-1, 0))) {
        load_calibration();
    }
    
    ImGui::Separator();
    
    // Status
    if (g_app.is_calibrated) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "✓ Calibrated");
        ImGui::Text("A = %.4f nm/px", g_app.cal_A);
        ImGui::Text("B = %.2f nm", g_app.cal_B);
        ImGui::Text("Range: %.1f - %.1f nm", 
            g_app.wavelength_from_pixel(0), 
            g_app.wavelength_from_pixel(CCD_PIXEL_COUNT-1));
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f), "Not Calibrated");
    }
    
    ImGui::End();
}

void draw_snapshot_panel() {
    ImGui::Begin("Snapshot & Export");
    
    // Freeze/Unfreeze
    if (g_app.is_frozen) {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "● FROZEN");
        if (ImGui::Button("Resume Live", ImVec2(-1, 0))) {
            unfreeze_frame();
        }
    } else {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "● LIVE");
        if (ImGui::Button("Freeze Frame", ImVec2(-1, 0))) {
            freeze_frame();
        }
    }
    
    ImGui::Separator();
    
    // Export
    ImGui::Text("Export Folder:");
    ImGui::InputText("##folder", g_app.export_folder, sizeof(g_app.export_folder));
    
    if (g_app.is_frozen) {
        if (ImGui::Button("Save Snapshot", ImVec2(-1, 0))) {
            export_snapshot();
        }
        ImGui::TextWrapped("Saves: CSV, peaks, metadata to timestamped folder");
    } else {
        ImGui::TextDisabled("Freeze frame first to save");
    }
    
    ImGui::Separator();
    
    // Peak detection settings
    ImGui::Text("Peak Detection");
    ImGui::SliderFloat("Threshold", &g_app.peak_threshold, 100.0f, 4000.0f);
    ImGui::SliderInt("Min Distance", &g_app.peak_min_distance, 10, 200);
    
    if (g_app.is_frozen && ImGui::Button("Re-detect Peaks", ImVec2(-1, 0))) {
        detect_peaks();
    }
    
    // Peak list
    if (!g_app.detected_peaks.empty()) {
        ImGui::Separator();
        ImGui::Text("Detected Peaks (%zu):", g_app.detected_peaks.size());
        
        ImGui::BeginChild("PeakList", ImVec2(0, 150), true);
        for (size_t i = 0; i < g_app.detected_peaks.size(); i++) {
            const auto& p = g_app.detected_peaks[i];
            if (g_app.is_calibrated) {
                ImGui::Text("%zu: px=%d, %.1f nm, I=%.0f", i+1, p.pixel_index, p.wavelength, p.intensity);
            } else {
                ImGui::Text("%zu: px=%d, I=%.0f", i+1, p.pixel_index, p.intensity);
            }
        }
        ImGui::EndChild();
    }
    
    ImGui::End();
}

void draw_spectrum_chart() {
    ImGui::Begin("Spectrum");
    
    // Status indicator
    if (g_app.is_frozen) {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "● FROZEN");
        ImGui::SameLine();
    } else if (g_app.connection_mode != ConnectionMode::None && g_app.packets_per_second > 0) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "● LIVE");
        ImGui::SameLine();
    }
    ImGui::Text("TCD1304 CCD Spectrum (%d pixels)", CCD_PIXEL_COUNT);
    
    ImVec2 plot_size = ImVec2(-1, -1);
    
    if (ImPlot::BeginPlot("##Spectrum", plot_size)) {
        // Primary X-axis: Pixel Index
        ImPlot::SetupAxes("Pixel Index", "Intensity (12-bit)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, CCD_PIXEL_COUNT, ImPlotCond_Once);
        
        // Secondary X-axis: Wavelength (if calibrated)
        if (g_app.is_calibrated && g_app.show_wavelength_axis) {
            ImPlot::SetupAxis(ImAxis_X2, "Wavelength (nm)");
            ImPlot::SetupAxisLimits(ImAxis_X2, 
                g_app.wavelength_from_pixel(0), 
                g_app.wavelength_from_pixel(CCD_PIXEL_COUNT-1), 
                ImPlotCond_Always);
            ImPlot::SetupAxisLinks(ImAxis_X2, nullptr, nullptr);
        }
        
        if (g_app.auto_fit_y) {
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 4095, ImPlotCond_Once);
        } else {
            ImPlot::SetupAxisLimits(ImAxis_Y1, g_app.y_min, g_app.y_max, ImPlotCond_Always);
        }
        
        // Plot spectrum data
        {
            std::lock_guard<std::mutex> lock(g_app.data_mutex);
            const std::vector<float>& data = g_app.is_frozen ? g_app.snapshot_data : g_app.spectrum_data;
            ImPlot::PlotLine("Intensity", g_app.pixel_indices.data(), data.data(), CCD_PIXEL_COUNT);
        }
        
        // Plot peak markers
        if (g_app.show_peaks && !g_app.detected_peaks.empty()) {
            std::vector<float> peak_x, peak_y;
            for (const auto& p : g_app.detected_peaks) {
                peak_x.push_back(static_cast<float>(p.pixel_index));
                peak_y.push_back(p.intensity);
            }
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond, 8, ImVec4(1, 0.3f, 0.3f, 1));
            ImPlot::PlotScatter("Peaks", peak_x.data(), peak_y.data(), (int)peak_x.size());
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
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.22f, &dock_left, &dock_main);
    
    ImGuiID dock_left_top, dock_left_mid, dock_left_bot;
    ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.5f, &dock_left_bot, &dock_left_top);
    ImGui::DockBuilderSplitNode(dock_left_top, ImGuiDir_Down, 0.5f, &dock_left_mid, &dock_left_top);
    
    ImGuiID dock_center, dock_bottom;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_center);
    
    ImGui::DockBuilderDockWindow("Controls", dock_left_top);
    ImGui::DockBuilderDockWindow("Calibration", dock_left_mid);
    ImGui::DockBuilderDockWindow("Snapshot & Export", dock_left_bot);
    ImGui::DockBuilderDockWindow("Spectrum", dock_center);
    ImGui::DockBuilderDockWindow("Console", dock_bottom);
    
    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace lgplot
