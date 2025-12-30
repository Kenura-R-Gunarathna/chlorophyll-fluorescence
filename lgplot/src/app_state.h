/*
 * Application State
 * Shared state container for the application.
 */
#pragma once

#include "config.h"

#include <vector>
#include <string>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#endif

namespace lgplot {

struct AppState {
    // Spectrum data
    std::vector<float> spectrum_data;
    std::vector<float> pixel_indices;
    std::mutex data_mutex;
    
    // Connection
    ConnectionMode connection_mode = ConnectionMode::None;
    std::atomic<bool> receiver_running{false};
    std::thread receiver_thread;
    
    // UDP
    SOCKET udp_socket = INVALID_SOCKET;
    
    // USB Serial
    HANDLE serial_handle = INVALID_HANDLE_VALUE;
    char com_port[32] = "COM3";
    
    // Statistics
    std::atomic<uint32_t> packets_received{0};
    std::atomic<uint32_t> last_sequence{0};
    std::atomic<float> packets_per_second{0.0f};
    
    // Console log
    std::deque<std::string> console_log;
    std::mutex log_mutex;
    
    // UI state
    bool auto_fit_y = true;
    float y_min = 0.0f;
    float y_max = 4095.0f;
    bool show_grid = true;
    bool dark_theme = true;
    
    AppState() {
        spectrum_data.resize(CCD_PIXEL_COUNT, 0.0f);
        pixel_indices.resize(CCD_PIXEL_COUNT);
        for (int i = 0; i < CCD_PIXEL_COUNT; ++i) {
            pixel_indices[i] = static_cast<float>(i);
        }
    }
};

// Global application state
extern AppState g_app;

} // namespace lgplot
