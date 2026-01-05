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
// SNAPSHOT HISTORY VIEWER
// ============================================
void scan_snapshot_folders() {
    g_app.snapshot_timestamps.clear();
    
    std::string folder_path = g_app.export_folder;
    if (!std::filesystem::exists(folder_path)) {
        return;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            // Check if it looks like a timestamp folder (YYYY-MM-DD_HH-MM-SS)
            if (name.length() >= 10 && name[4] == '-' && name[7] == '-') {
                g_app.snapshot_timestamps.push_back(name);
            }
        }
    }
    
    // Sort in reverse order (newest first)
    std::sort(g_app.snapshot_timestamps.rbegin(), g_app.snapshot_timestamps.rend());
    
    log_message("Found %zu snapshots in '%s'", g_app.snapshot_timestamps.size(), folder_path.c_str());
}

bool load_snapshot_history(int index) {
    if (index < 0 || index >= (int)g_app.snapshot_timestamps.size()) {
        g_app.history_loaded = false;
        return false;
    }
    
    std::string folder = std::string(g_app.export_folder) + "/" + g_app.snapshot_timestamps[index];
    std::string csv_path = folder + "/spectrum_data.csv";
    
    g_app.history_data.clear();
    g_app.history_data.resize(CCD_PIXEL_COUNT, 0.0f);
    g_app.history_peaks.clear();
    
    // Load spectrum CSV
    std::ifstream csv(csv_path);
    if (csv.is_open()) {
        std::string line;
        std::getline(csv, line); // Skip header
        
        while (std::getline(csv, line)) {
            std::stringstream ss(line);
            std::string pixel_str, wave_str, intensity_str;
            
            if (std::getline(ss, pixel_str, ',') &&
                std::getline(ss, wave_str, ',') &&
                std::getline(ss, intensity_str, ',')) {
                int pixel = std::stoi(pixel_str);
                float intensity = std::stof(intensity_str);
                if (pixel >= 0 && pixel < CCD_PIXEL_COUNT) {
                    g_app.history_data[pixel] = intensity;
                }
            }
        }
        csv.close();
    } else {
        log_message("ERROR: Could not load %s", csv_path.c_str());
        g_app.history_loaded = false;
        return false;
    }
    
    // Load peaks CSV
    std::string peaks_path = folder + "/peaks.csv";
    std::ifstream peaks_csv(peaks_path);
    if (peaks_csv.is_open()) {
        std::string line;
        std::getline(peaks_csv, line); // Skip header
        
        while (std::getline(peaks_csv, line)) {
            std::stringstream ss(line);
            std::string num_str, pixel_str, wave_str, intensity_str;
            
            if (std::getline(ss, num_str, ',') &&
                std::getline(ss, pixel_str, ',') &&
                std::getline(ss, wave_str, ',') &&
                std::getline(ss, intensity_str, ',')) {
                Peak p;
                p.pixel_index = std::stoi(pixel_str);
                p.wavelength = std::stof(wave_str);
                p.intensity = std::stof(intensity_str);
                g_app.history_peaks.push_back(p);
            }
        }
        peaks_csv.close();
    }
    
    g_app.history_selected_index = index;
    g_app.history_loaded = true;
    log_message("Loaded snapshot: %s (%zu peaks)", g_app.snapshot_timestamps[index].c_str(), g_app.history_peaks.size());
    return true;
}

void draw_history_viewer_panel() {
    ImGui::Begin("Snapshot History");
    
    // Refresh button
    if (ImGui::Button("Scan for Snapshots", ImVec2(-1, 0))) {
        scan_snapshot_folders();
    }
    
    if (g_app.snapshot_timestamps.empty()) {
        ImGui::TextDisabled("No snapshots found in '%s'", g_app.export_folder);
        ImGui::End();
        return;
    }
    
    ImGui::Text("Found %zu snapshots", g_app.snapshot_timestamps.size());
    ImGui::Separator();
    
    // Method 1: Slider
    ImGui::Text("Slider Navigation:");
    int slider_val = g_app.history_selected_index >= 0 ? g_app.history_selected_index : 0;
    if (ImGui::SliderInt("##slider", &slider_val, 0, (int)g_app.snapshot_timestamps.size() - 1)) {
        load_snapshot_history(slider_val);
    }
    
    // Method 2: Dropdown
    ImGui::Text("Dropdown Selection:");
    const char* preview = g_app.history_selected_index >= 0 ? 
        g_app.snapshot_timestamps[g_app.history_selected_index].c_str() : "Select snapshot...";
    
    if (ImGui::BeginCombo("##dropdown", preview)) {
        for (int i = 0; i < (int)g_app.snapshot_timestamps.size(); i++) {
            bool is_selected = (i == g_app.history_selected_index);
            if (ImGui::Selectable(g_app.snapshot_timestamps[i].c_str(), is_selected)) {
                load_snapshot_history(i);
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::Separator();
    
    // Current selection info
    if (g_app.history_loaded && g_app.history_selected_index >= 0) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Loaded: %s", 
            g_app.snapshot_timestamps[g_app.history_selected_index].c_str());
        ImGui::Text("Peaks: %zu", g_app.history_peaks.size());
        
        // Peak list
        if (!g_app.history_peaks.empty()) {
            ImGui::BeginChild("HistoryPeaks", ImVec2(0, 100), true);
            for (size_t i = 0; i < g_app.history_peaks.size(); i++) {
                const auto& p = g_app.history_peaks[i];
                ImGui::Text("%zu: px=%d, %.1fnm, I=%.0f", i+1, p.pixel_index, p.wavelength, p.intensity);
            }
            ImGui::EndChild();
        }
    } else {
        ImGui::TextDisabled("Select a snapshot to view");
    }
    
    ImGui::End();
}

void draw_history_chart() {
    ImGui::Begin("History Preview");
    
    if (!g_app.history_loaded) {
        ImGui::TextDisabled("No snapshot loaded. Use 'Snapshot History' panel to select one.");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Snapshot: %s", g_app.snapshot_timestamps[g_app.history_selected_index].c_str());
    
    ImVec2 plot_size = ImVec2(-1, -1);
    
    if (ImPlot::BeginPlot("##HistoryPlot", plot_size)) {
        ImPlot::SetupAxes("Pixel Index", "Intensity (12-bit)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, CCD_PIXEL_COUNT, ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 4095, ImPlotCond_Once);
        
        // Secondary wavelength axis if calibrated
        if (g_app.is_calibrated && g_app.show_wavelength_axis) {
            ImPlot::SetupAxis(ImAxis_X2, "Wavelength (nm)");
            ImPlot::SetupAxisLimits(ImAxis_X2, 
                g_app.wavelength_from_pixel(0), 
                g_app.wavelength_from_pixel(CCD_PIXEL_COUNT-1), 
                ImPlotCond_Always);
        }
        
        // Plot historical data
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
        ImPlot::PlotLine("History", g_app.pixel_indices.data(), g_app.history_data.data(), CCD_PIXEL_COUNT);
        ImPlot::PopStyleColor();
        
        // Plot peaks
        if (g_app.show_peaks && !g_app.history_peaks.empty()) {
            std::vector<float> peak_x, peak_y;
            for (const auto& p : g_app.history_peaks) {
                peak_x.push_back(static_cast<float>(p.pixel_index));
                peak_y.push_back(p.intensity);
            }
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond, 8, ImVec4(1, 0.5f, 0.2f, 1));
            ImPlot::PlotScatter("Peaks", peak_x.data(), peak_y.data(), (int)peak_x.size());
        }
        
        ImPlot::EndPlot();
    }
    
    ImGui::End();
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
    ImGui::InputInt("px-1(idx)", &g_app.cal_point1.pixel_index);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Pixel index on the sensor or image\n"
            "corresponding to calibration point 1.\n\n"
            "Example: 512"
        );
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputFloat("wl-1(nm)", &g_app.cal_point1.wavelength_nm, 0.0f, 0.0f, "%.1f");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Known wavelength in nanometers (nm)\n"
            "for calibration point 1.\n\n"
            "Example: 589.0 nm (Na D-line)"
        );
    }

    // Calibration point 2
    ImGui::Text("Point 2:");
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("px-2(idx)", &g_app.cal_point2.pixel_index);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Pixel index on the sensor or image\n"
            "corresponding to calibration point 2.\n\n"
            "Example: 1024"
        );
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputFloat("wl-2(nm)", &g_app.cal_point2.wavelength_nm, 0.0f, 0.0f, "%.1f");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Known wavelength in nanometers (nm)\n"
            "for calibration point 2.\n\n"
            "Example: 656.3 nm (Hα line)"
        );
    }

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
    
    // Split: Left panel | Main area
    ImGuiID dock_left, dock_main;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.22f, &dock_left, &dock_main);
    
    // Left panel: 4 sections stacked
    ImGuiID dock_left_1, dock_left_2, dock_left_3, dock_left_4;
    ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.75f, &dock_left_4, &dock_left_1);
    ImGui::DockBuilderSplitNode(dock_left_4, ImGuiDir_Down, 0.66f, &dock_left_4, &dock_left_2);
    ImGui::DockBuilderSplitNode(dock_left_4, ImGuiDir_Down, 0.5f, &dock_left_4, &dock_left_3);
    
    // Main area: Charts on top, Console on bottom
    ImGuiID dock_charts, dock_bottom;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_charts);
    
    // Dock windows
    ImGui::DockBuilderDockWindow("Controls", dock_left_1);
    ImGui::DockBuilderDockWindow("Calibration", dock_left_2);
    ImGui::DockBuilderDockWindow("Snapshot & Export", dock_left_3);
    ImGui::DockBuilderDockWindow("Snapshot History", dock_left_4);
    
    // Charts as tabs (Spectrum and History Preview side by side or tabbed)
    ImGui::DockBuilderDockWindow("Spectrum", dock_charts);
    ImGui::DockBuilderDockWindow("History Preview", dock_charts);  // Tabbed with Spectrum
    
    ImGui::DockBuilderDockWindow("Console", dock_bottom);
    
    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace lgplot
