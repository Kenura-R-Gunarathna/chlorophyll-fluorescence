/*
 * Connection Module Implementation
 * USB Serial and UDP receiver threads.
 */
#include "connection.h"
#include "app_state.h"
#include "console.h"

#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

namespace lgplot {

// ============================================
// USB SERIAL
// ============================================
bool open_serial_port() {
    char port_path[64];
    snprintf(port_path, sizeof(port_path), "\\\\.\\%s", g_app.com_port);
    
    g_app.serial_handle = CreateFileA(
        port_path,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL,
        OPEN_EXISTING,
        0, NULL
    );
    
    if (g_app.serial_handle == INVALID_HANDLE_VALUE) {
        log_message("ERROR: Failed to open %s", g_app.com_port);
        return false;
    }
    
    // Configure serial port
    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(g_app.serial_handle, &dcb);
    dcb.BaudRate = USB_BAUD_RATE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    SetCommState(g_app.serial_handle, &dcb);
    
    // Set timeouts
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 10;
    timeouts.ReadTotalTimeoutConstant = 10;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(g_app.serial_handle, &timeouts);
    
    // Purge buffers
    PurgeComm(g_app.serial_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    
    log_message("Opened %s @ %d baud", g_app.com_port, USB_BAUD_RATE);
    return true;
}

void close_serial_port() {
    if (g_app.serial_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(g_app.serial_handle);
        g_app.serial_handle = INVALID_HANDLE_VALUE;
    }
}

static void usb_receiver_thread() {
    log_message("USB Serial receiver started on %s", g_app.com_port);
    
    std::vector<uint8_t> buffer(8 + CCD_PIXEL_COUNT * 2 + 100);
    size_t buffer_pos = 0;
    
    auto last_stats_time = std::chrono::steady_clock::now();
    uint32_t packets_since_last = 0;
    
    while (g_app.receiver_running) {
        // Read into buffer
        DWORD bytes_read = 0;
        if (ReadFile(g_app.serial_handle, buffer.data() + buffer_pos, 
                    (DWORD)(buffer.size() - buffer_pos), &bytes_read, NULL)) {
            buffer_pos += bytes_read;
        }
        
        // Look for complete packets: [0xAA] [seq_hi] [seq_lo] [cnt_hi] [cnt_lo] [data...] [0x55]
        while (buffer_pos >= 6) {
            // Find start marker
            size_t start = 0;
            while (start < buffer_pos && buffer[start] != USB_START_MARKER) {
                start++;
            }
            
            if (start > 0) {
                // Shift buffer
                memmove(buffer.data(), buffer.data() + start, buffer_pos - start);
                buffer_pos -= start;
            }
            
            if (buffer_pos < 6) break;
            
            // Parse header
            uint16_t pixel_count = (buffer[3] << 8) | buffer[4];
            size_t expected_size = 5 + pixel_count * 2 + 1;  // header + data + end marker
            
            if (pixel_count > CCD_PIXEL_COUNT) {
                // Invalid count, skip this byte
                memmove(buffer.data(), buffer.data() + 1, buffer_pos - 1);
                buffer_pos--;
                continue;
            }
            
            if (buffer_pos < expected_size) break;  // Wait for more data
            
            // Verify end marker
            if (buffer[expected_size - 1] == USB_END_MARKER) {
                uint16_t sequence = (buffer[1] << 8) | buffer[2];
                uint16_t* pixels = reinterpret_cast<uint16_t*>(&buffer[5]);
                
                {
                    std::lock_guard<std::mutex> lock(g_app.data_mutex);
                    for (uint16_t i = 0; i < pixel_count; ++i) {
                        g_app.spectrum_data[i] = static_cast<float>(pixels[i]);
                    }
                }
                
                g_app.packets_received++;
                g_app.last_sequence = sequence;
                packets_since_last++;
            }
            
            // Remove processed packet
            memmove(buffer.data(), buffer.data() + expected_size, buffer_pos - expected_size);
            buffer_pos -= expected_size;
        }
        
        // Update packets per second
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_stats_time).count();
        if (elapsed >= 1000) {
            g_app.packets_per_second = packets_since_last * 1000.0f / elapsed;
            packets_since_last = 0;
            last_stats_time = now;
        }
    }
    
    log_message("USB Serial receiver stopped");
}

bool start_usb_receiver() {
    if (!open_serial_port()) return false;
    
    g_app.receiver_running = true;
    g_app.connection_mode = ConnectionMode::USB;
    g_app.receiver_thread = std::thread(usb_receiver_thread);
    return true;
}

// ============================================
// UDP RECEIVER
// ============================================
#pragma pack(push, 1)
struct UdpPacketHeader {
    uint8_t  start_byte;
    uint32_t sequence_num;
    uint32_t timestamp_us;
    uint16_t pixel_count;
    uint16_t checksum;
};
#pragma pack(pop)

static void udp_receiver_thread() {
    log_message("UDP receiver started on port %d", UDP_PORT);
    
    std::vector<uint8_t> buffer(sizeof(UdpPacketHeader) + CCD_PIXEL_COUNT * sizeof(uint16_t) + 100);
    
    auto last_stats_time = std::chrono::steady_clock::now();
    uint32_t packets_since_last = 0;
    
    while (g_app.receiver_running) {
        sockaddr_in sender_addr;
        int sender_len = sizeof(sender_addr);
        
        int bytes = recvfrom(g_app.udp_socket, (char*)buffer.data(), (int)buffer.size(), 0,
                            (sockaddr*)&sender_addr, &sender_len);
        
        if (bytes > 0 && bytes >= (int)sizeof(UdpPacketHeader)) {
            if (buffer[0] == 0xAA) {
                auto* header = reinterpret_cast<UdpPacketHeader*>(buffer.data());
                
                if (header->pixel_count <= CCD_PIXEL_COUNT) {
                    uint16_t* pixels = reinterpret_cast<uint16_t*>(buffer.data() + sizeof(UdpPacketHeader));
                    
                    {
                        std::lock_guard<std::mutex> lock(g_app.data_mutex);
                        for (uint16_t i = 0; i < header->pixel_count; ++i) {
                            g_app.spectrum_data[i] = static_cast<float>(pixels[i]);
                        }
                    }
                    
                    g_app.packets_received++;
                    g_app.last_sequence = header->sequence_num;
                    packets_since_last++;
                }
            }
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_stats_time).count();
        if (elapsed >= 1000) {
            g_app.packets_per_second = packets_since_last * 1000.0f / elapsed;
            packets_since_last = 0;
            last_stats_time = now;
        }
    }
    
    log_message("UDP receiver stopped");
}

bool start_udp_receiver() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        log_message("ERROR: WSAStartup failed");
        return false;
    }
    
    g_app.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_app.udp_socket == INVALID_SOCKET) {
        log_message("ERROR: Failed to create socket");
        WSACleanup();
        return false;
    }
    
    DWORD timeout = 10;
    setsockopt(g_app.udp_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    int rcvbuf = 256 * 1024;
    setsockopt(g_app.udp_socket, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf, sizeof(rcvbuf));
    
    sockaddr_in local_addr = {};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(UDP_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(g_app.udp_socket, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
        log_message("ERROR: Failed to bind to port %d", UDP_PORT);
        closesocket(g_app.udp_socket);
        WSACleanup();
        return false;
    }
    
    g_app.receiver_running = true;
    g_app.connection_mode = ConnectionMode::UDP;
    g_app.receiver_thread = std::thread(udp_receiver_thread);
    
    log_message("Listening for UDP on port %d", UDP_PORT);
    return true;
}

// ============================================
// CONNECTION MANAGEMENT
// ============================================
void stop_receiver() {
    g_app.receiver_running = false;
    if (g_app.receiver_thread.joinable()) {
        g_app.receiver_thread.join();
    }
    
    if (g_app.connection_mode == ConnectionMode::UDP) {
        if (g_app.udp_socket != INVALID_SOCKET) {
            closesocket(g_app.udp_socket);
            g_app.udp_socket = INVALID_SOCKET;
        }
        WSACleanup();
    } else if (g_app.connection_mode == ConnectionMode::USB) {
        close_serial_port();
    }
    
    g_app.connection_mode = ConnectionMode::None;
}

} // namespace lgplot
