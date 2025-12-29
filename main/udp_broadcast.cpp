/*
 * UDP Broadcast - Implementation
 * ==============================
 * Broadcasts CCD pixel data over UDP to all devices on the network.
 */

#include "udp_broadcast.h"
#include "config.h"

#include <string.h>
#include "lwip/sockets.h"
#include "esp_log.h"
#include "esp_timer.h"

// Only include wifi_ap for AP mode
#if CONNECTION_MODE == CONNECTION_MODE_WIFI_AP
#include "wifi_ap.h"
#endif

static const char* TAG = "UDP_BROADCAST";

static int udp_socket = -1;
static struct sockaddr_in broadcast_addr;
static uint32_t packet_sequence = 0;

// ============================================
// CHECKSUM CALCULATION
// ============================================
static uint16_t calculate_checksum(const uint16_t* data, size_t count) {
    uint32_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += data[i];
    }
    return (uint16_t)(sum & 0xFFFF);
}

// ============================================
// PUBLIC: Initialize UDP socket
// ============================================
void udp_broadcast_init(void) {
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return;
    }

    // Enable broadcast
    int broadcast_enable = 1;
    setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST,
               &broadcast_enable, sizeof(broadcast_enable));

    // Configure broadcast address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(UDP_PORT);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;  // 255.255.255.255

    ESP_LOGI(TAG, "UDP socket ready, broadcasting to port %d", UDP_PORT);
}

// ============================================
// PUBLIC: Send pixel data
// ============================================
bool udp_broadcast_send(const uint16_t* buffer, size_t count) {
    if (udp_socket < 0) {
        return false;
    }

    // In AP mode, skip if no clients connected
#if CONNECTION_MODE == CONNECTION_MODE_WIFI_AP
    if (wifi_ap_get_client_count() == 0) {
        return false;
    }
#endif
    // In Station mode, always send (receiver is on same network)

    // Static packet buffer (avoid malloc overhead)
    static uint8_t packet[sizeof(UdpPacketHeader) + CCD_PIXEL_COUNT * sizeof(uint16_t)];

    // Prepare header
    UdpPacketHeader* header = (UdpPacketHeader*)packet;
    header->start_byte = 0xAA;
    header->sequence_num = packet_sequence++;
    header->timestamp_us = (uint32_t)esp_timer_get_time();
    header->pixel_count = count;
    header->checksum = calculate_checksum(buffer, count);

    // Copy pixel data directly after header
    memcpy(packet + sizeof(UdpPacketHeader), buffer, count * sizeof(uint16_t));

    // Send
    size_t packet_size = sizeof(UdpPacketHeader) + (count * sizeof(uint16_t));
    int sent = sendto(udp_socket, packet, packet_size, 0,
                      (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

    if (sent < 0) {
        return false;
    }

    // Log every 500 packets
    if (packet_sequence % 500 == 0) {
        ESP_LOGI(TAG, "Sent %lu packets (%d bytes each)",
                 (unsigned long)packet_sequence, (int)packet_size);
    }

    return true;
}
