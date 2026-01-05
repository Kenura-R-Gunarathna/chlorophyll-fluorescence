/*
 * Connection Module Implementation
 * USB Serial and UDP receiver threads.
 *
 * USB Serial Protocol (Partner's format):
 * - 0x11 = New data frame indicator
 * - For each pixel: [0xA5][lowByte][highByte][0x5A]
 * - Baud rate: 1,000,000 (1 Mbps)
 *
 * Cross-platform support:
 * - Windows: Winsock + CreateFile for serial
 * - Linux: POSIX sockets + termios for serial
 */
#include "connection.h"
#include "app_state.h"
#include "console.h"

#include <chrono>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
// Linux/Unix includes
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#define closesocket close
#define SOCKET_ERROR (-1)
#endif

namespace lgplot {

// ============================================
// USB SERIAL
// ============================================
#ifdef _WIN32
// Windows serial port implementation
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
  dcb.BaudRate = USB_BAUD_RATE;
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

#else
// Linux serial port implementation using termios
bool open_serial_port() {
  // On Linux, com_port should be like "/dev/ttyUSB0" or "/dev/ttyACM0"
  // If user set "COM3", convert to Linux format
  char port_path[64];
  if (strncmp(g_app.com_port, "COM", 3) == 0) {
    int port_num = atoi(g_app.com_port + 3);
    snprintf(port_path, sizeof(port_path), "/dev/ttyUSB%d", port_num - 1);
  } else if (g_app.com_port[0] != '/') {
    snprintf(port_path, sizeof(port_path), "/dev/%s", g_app.com_port);
  } else {
    strncpy(port_path, g_app.com_port, sizeof(port_path));
  }

  g_app.serial_handle = open(port_path, O_RDWR | O_NOCTTY | O_NONBLOCK);

  if (g_app.serial_handle < 0) {
    log_message("ERROR: Failed to open %s: %s", port_path, strerror(errno));
    return false;
  }

  // Configure serial port with termios
  struct termios tty;
  memset(&tty, 0, sizeof(tty));

  if (tcgetattr(g_app.serial_handle, &tty) != 0) {
    log_message("ERROR: tcgetattr failed");
    close(g_app.serial_handle);
    g_app.serial_handle = INVALID_HANDLE_VALUE;
    return false;
  }

  // Set baud rate (1000000 = B1000000 on Linux)
  cfsetispeed(&tty, B1000000);
  cfsetospeed(&tty, B1000000);

  // 8N1 mode
  tty.c_cflag &= ~PARENB; // No parity
  tty.c_cflag &= ~CSTOPB; // 1 stop bit
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;            // 8 data bits
  tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
  tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines

  // Raw input mode
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

  // Raw output mode
  tty.c_oflag &= ~OPOST;

  // Non-blocking read with minimal timeout
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 1; // 100ms timeout

  if (tcsetattr(g_app.serial_handle, TCSANOW, &tty) != 0) {
    log_message("ERROR: tcsetattr failed");
    close(g_app.serial_handle);
    g_app.serial_handle = INVALID_HANDLE_VALUE;
    return false;
  }

  // Flush buffers
  tcflush(g_app.serial_handle, TCIOFLUSH);

  log_message("Opened %s @ %d baud", port_path, USB_BAUD_RATE);
  return true;
}

void close_serial_port() {
  if (g_app.serial_handle != INVALID_HANDLE_VALUE) {
    close(g_app.serial_handle);
    g_app.serial_handle = INVALID_HANDLE_VALUE;
  }
}
#endif

/**
 * USB Serial receiver thread - Partner's protocol (cross-platform)
 */
static void usb_receiver_thread() {
  log_message("USB Serial receiver started on %s", g_app.com_port);
  log_message("Protocol: 0x11=frame, 0xA5+data+0x5A=pixel");

  std::vector<uint8_t> buffer(16384);
  size_t buffer_pos = 0;

  int pixel_index = 0;
  bool receiving_frame = false;
  std::vector<float> temp_pixels(CCD_PIXEL_COUNT, 0.0f);

  auto last_stats_time = std::chrono::steady_clock::now();
  uint32_t packets_since_last = 0;
  uint32_t pixels_this_frame = 0;

  while (g_app.receiver_running) {
    uint8_t byte;

#ifdef _WIN32
    DWORD bytes_read = 0;
    if (!ReadFile(g_app.serial_handle, &byte, 1, &bytes_read, NULL) ||
        bytes_read == 0) {
      Sleep(1);
      continue;
    }
#else
    ssize_t bytes_read = read(g_app.serial_handle, &byte, 1);
    if (bytes_read <= 0) {
      usleep(1000); // 1ms
      continue;
    }
#endif

    // Add byte to buffer
    if (buffer_pos < buffer.size()) {
      buffer[buffer_pos++] = byte;
    }

    // Check for frame start marker (0x11)
    if (byte == USB_FRAME_START) {
      if (receiving_frame && pixel_index > 0) {
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

      receiving_frame = true;
      pixel_index = 0;
      pixels_this_frame = 0;
      buffer_pos = 0;
      continue;
    }

    if (!receiving_frame) {
      buffer_pos = 0;
      continue;
    }

    // Look for pixel packets: [0xA5][lowByte][highByte][0x5A]
    while (buffer_pos >= 4) {
      size_t start = 0;
      while (start < buffer_pos && buffer[start] != USB_PIXEL_START) {
        start++;
      }

      if (start > 0) {
        memmove(buffer.data(), buffer.data() + start, buffer_pos - start);
        buffer_pos -= start;
      }

      if (buffer_pos < 4)
        break;

      if (buffer[0] == USB_PIXEL_START && buffer[3] == USB_PIXEL_END) {
        uint8_t low_byte = buffer[1];
        uint8_t high_byte = buffer[2];
        uint16_t pixel_value = low_byte | (high_byte << 8);

        if (pixel_index < CCD_PIXEL_COUNT) {
          temp_pixels[pixel_index] = static_cast<float>(pixel_value);
          pixel_index++;
          pixels_this_frame++;
        }

        memmove(buffer.data(), buffer.data() + 4, buffer_pos - 4);
        buffer_pos -= 4;
      } else {
        memmove(buffer.data(), buffer.data() + 1, buffer_pos - 1);
        buffer_pos--;
      }
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - last_stats_time)
                       .count();
    if (elapsed >= 1000) {
      g_app.packets_per_second = packets_since_last * 1000.0f / elapsed;
      g_app.last_sequence = pixels_this_frame;
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
// UDP RECEIVER (Cross-platform)
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
    socklen_t sender_len = sizeof(sender_addr);

    ssize_t bytes =
        recvfrom(g_app.udp_socket, (char *)buffer.data(), buffer.size(), 0,
                 (sockaddr *)&sender_addr, &sender_len);

    if (bytes > 0 && bytes >= (ssize_t)sizeof(UdpPacketHeader)) {
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
#ifdef _WIN32
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    log_message("ERROR: WSAStartup failed");
    return false;
  }
#endif

  g_app.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (g_app.udp_socket == INVALID_SOCKET) {
    log_message("ERROR: Failed to create socket");
#ifdef _WIN32
    WSACleanup();
#endif
    return false;
  }

  // Set receive timeout
#ifdef _WIN32
  DWORD timeout = 10;
  setsockopt(g_app.udp_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
             sizeof(timeout));
#else
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 10000; // 10ms
  setsockopt(g_app.udp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,
             sizeof(timeout));
#endif

  // Set receive buffer size
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
#ifdef _WIN32
    WSACleanup();
#endif
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
#ifdef _WIN32
    WSACleanup();
#endif
  } else if (g_app.connection_mode == ConnectionMode::USB) {
    close_serial_port();
  }

  g_app.connection_mode = ConnectionMode::None;
}

} // namespace lgplot
