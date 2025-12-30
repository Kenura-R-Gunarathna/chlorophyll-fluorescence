/*
 * Application Configuration
 * Contains global constants and configuration settings.
 */
#pragma once

#include <cstdint>

namespace lgplot {

// Network Configuration
constexpr int UDP_PORT = 8080;
constexpr int CCD_PIXEL_COUNT = 3694;

// Serial Configuration  
constexpr int USB_BAUD_RATE = 921600;
constexpr uint8_t USB_START_MARKER = 0xAA;
constexpr uint8_t USB_END_MARKER = 0x55;

// UI Configuration
constexpr const char* LAYOUT_INI_FILE = "lgplot_layout.ini";
constexpr size_t MAX_LOG_LINES = 200;

// Connection modes
enum class ConnectionMode {
    None = 0,
    UDP,
    USB
};

} // namespace lgplot
