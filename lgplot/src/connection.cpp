/*
 * Connection Module Implementation
 * USB Serial and UDP receiver threads.
 *
 * USB Serial Protocol (Partner's format):
 * - 0x11 = New data frame indicator
 * - For each pixel: [0xA5][lowByte][highByte][0x5A]
 * - Baud rate: 1,000,000 (1 Mbps)
 *
 * NOTE: Serial and UDP features are Windows-only for now.
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

#ifdef _WIN32
// ============================================
// USB SERIAL (Windows-only)
// ============================================
bool open_serial_port() {
  char port_path[64];
  snprintf(port_path, sizeof(port_path), "\\\\.\\%s", g_app.com_port);

  g_app.serial_handle = CreateFileA(port_path, GENERIC_READ | GENERIC_WRITE, 0,
                                    NULL, OPEN_EXISTING, 0, NULL);

  if (g_app.serial_handle == INVALID_HANDLE_VALUE) {
    log_message("ERROR: Failed to open %s", g_app.com_port);
    return false;
  }

  // Configure serial port
  DCB dcb = {};
  dcb.DCBlength = sizeof(DCB);
  GetCommState(g_app.serial_handle, &dcb);
  dcb.BaudRate = USB_BAUD_RATE; // 1,000,000 baud
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fParity = FALSE;
  SetCommState(g_app.serial_handle, &dcb);

  // Set timeouts
  COMMTIMEOUTS timeouts = {};
  timeouts.ReadIntervalTimeout = 1;
  timeouts.ReadTotalTimeoutConstant = 1;
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

/**
 * USB Serial receiver thread - Partner's protocol
 *
 * Protocol:
 * - 0x11 indicates start of new data frame
 * - Each pixel: [0xA5][lowByte][highByte][0x5A]
 * - 3694 pixels per frame
 */
static void usb_receiver_thread() {
  log_message("USB Serial receiver started on %s", g_app.com_port);
  log_message("Protocol: 0x11=frame, 0xA5+data+0x5A=pixel");

  std::vector<uint8_t> buffer(16384); // Large buffer for high-speed serial
  size_t buffer_pos = 0;

  int pixel_index = 0;
  bool receiving_frame = false;
  std::vector<float> temp_pixels(CCD_PIXEL_COUNT, 0.0f);

  auto last_stats_time = std::chrono::steady_clock::now();
  uint32_t packets_since_last = 0;
  uint32_t pixels_this_frame = 0;

  while (g_app.receiver_running) {
    // Read bytes from serial port
    DWORD bytes_read = 0;
    uint8_t byte;

    if (!ReadFile(g_app.serial_handle, &byte, 1, &bytes_read, NULL) ||
        bytes_read == 0) {
      // No data available, short sleep to prevent CPU spin
      Sleep(1);
      continue;
    }

    // Add byte to buffer
    if (buffer_pos < buffer.size()) {
      buffer[buffer_pos++] = byte;
    }

    // Check for frame start marker (0x11)
    if (byte == USB_FRAME_START) {
      if (receiving_frame && pixel_index > 0) {
        // Previous frame complete, copy to display
        {
          std::lock_guard<std::mutex> lock(g_app.data_mutex);
          for (int i = 0; i < pixel_index && i < CCD_PIXEL_COUNT; i++) {
            g_app.spectrum_data[i] = temp_pixels[i];
          }
        }
        g_app.packets_received++;
        packets_since_last++;
        log_message("Frame complete: %d pixels", pixel_index);
      }

      // Start new frame
      receiving_frame = true;
      pixel_index = 0;
      pixels_this_frame = 0;
      buffer_pos = 0;
      continue;
    }

    // If not receiving a frame, skip
    if (!receiving_frame) {
      buffer_pos = 0;
      continue;
    }

    // Look for pixel packets: [0xA5][lowByte][highByte][0x5A]
    // We need at least 4 bytes in buffer
    while (buffer_pos >= 4) {
      // Find 0xA5 start byte
      size_t start = 0;
      while (start < buffer_pos && buffer[start] != USB_PIXEL_START) {
        start++;
      }

      // Remove bytes before start marker
      if (start > 0) {
        memmove(buffer.data(), buffer.data() + start, buffer_pos - start);
        buffer_pos -= start;
      }

      // Need at least 4 bytes for a complete pixel packet
      if (buffer_pos < 4)
        break;

      // Check if this is a valid pixel packet
      if (buffer[0] == USB_PIXEL_START && buffer[3] == USB_PIXEL_END) {
        // Extract 12-bit pixel value (little-endian)
        uint8_t low_byte = buffer[1];
        uint8_t high_byte = buffer[2];
        uint16_t pixel_value = low_byte | (high_byte << 8);

        // Store pixel value
        if (pixel_index < CCD_PIXEL_COUNT) {
          temp_pixels[pixel_index] = static_cast<float>(pixel_value);
          pixel_index++;
          pixels_this_frame++;
        }

        // Remove processed packet
        memmove(buffer.data(), buffer.data() + 4, buffer_pos - 4);
        buffer_pos -= 4;
      } else {
        // Invalid packet, skip one byte and try again
        memmove(buffer.data(), buffer.data() + 1, buffer_pos - 1);
        buffer_pos--;
      }
    }

    // Update packets per second
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - last_stats_time)
                       .count();
    if (elapsed >= 1000) {
      g_app.packets_per_second = packets_since_last * 1000.0f / elapsed;
      g_app.last_sequence = pixels_this_frame; // Show pixels in current frame
      packets_since_last = 0;
      last_stats_time = now;
    }
  }

  log_message("USB Serial receiver stopped");
}

bool start_usb_receiver() {
  if (!open_serial_port())
    return false;

  g_app.receiver_running = true;
  g_app.connection_mode = ConnectionMode::USB;
  g_app.receiver_thread = std::thread(usb_receiver_thread);
  return true;
}

// ============================================
// UDP RECEIVER (Windows-only)
// ============================================
#pragma pack(push, 1)
struct UdpPacketHeader {
  uint8_t start_byte;
  uint32_t sequence_num;
  uint32_t timestamp_us;
  uint16_t pixel_count;
  uint16_t checksum;
};
#pragma pack(pop)

static void udp_receiver_thread() {
  log_message("UDP receiver started on port %d", UDP_PORT);

  std::vector<uint8_t> buffer(sizeof(UdpPacketHeader) +
                              CCD_PIXEL_COUNT * sizeof(uint16_t) + 100);

  auto last_stats_time = std::chrono::steady_clock::now();
  uint32_t packets_since_last = 0;

  while (g_app.receiver_running) {
    sockaddr_in sender_addr;
    int sender_len = sizeof(sender_addr);

    int bytes =
        recvfrom(g_app.udp_socket, (char *)buffer.data(), (int)buffer.size(), 0,
                 (sockaddr *)&sender_addr, &sender_len);

    if (bytes > 0 && bytes >= (int)sizeof(UdpPacketHeader)) {
      if (buffer[0] == 0xAA) {
        auto *header = reinterpret_cast<UdpPacketHeader *>(buffer.data());

        if (header->pixel_count <= CCD_PIXEL_COUNT) {
          uint16_t *pixels = reinterpret_cast<uint16_t *>(
              buffer.data() + sizeof(UdpPacketHeader));

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
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - last_stats_time)
                       .count();
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
  setsockopt(g_app.udp_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
             sizeof(timeout));

  int rcvbuf = 256 * 1024;
  setsockopt(g_app.udp_socket, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf,
             sizeof(rcvbuf));

  sockaddr_in local_addr = {};
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(UDP_PORT);
  local_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(g_app.udp_socket, (sockaddr *)&local_addr, sizeof(local_addr)) ==
      SOCKET_ERROR) {
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
// CONNECTION MANAGEMENT (Windows)
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

#else
// ============================================
// LINUX STUBS (Not implemented yet)
// ============================================
bool open_serial_port() {
  log_message("ERROR: Serial port not supported on Linux");
  return false;
}

void close_serial_port() {
  // No-op on Linux
}

bool start_usb_receiver() {
  log_message("ERROR: USB receiver not supported on Linux");
  return false;
}

bool start_udp_receiver() {
  log_message("ERROR: UDP receiver not supported on Linux");
  return false;
}

void stop_receiver() {
  g_app.receiver_running = false;
  if (g_app.receiver_thread.joinable()) {
    g_app.receiver_thread.join();
  }
  g_app.connection_mode = ConnectionMode::None;
}

#endif // _WIN32

} // namespace lgplot
