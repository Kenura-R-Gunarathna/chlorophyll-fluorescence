/*
 * UDP Broadcast - Header
 * ======================
 * Broadcasts CCD pixel data over UDP to all connected clients.
 */

#ifndef UDP_BROADCAST_H
#define UDP_BROADCAST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Packet header structure (sent before pixel data)
 */
typedef struct __attribute__((packed)) {
    uint8_t  start_byte;     // 0xAA magic byte
    uint32_t sequence_num;   // Packet sequence number
    uint32_t timestamp_us;   // Microsecond timestamp
    uint16_t pixel_count;    // Number of pixels in payload
    uint16_t checksum;       // Simple sum checksum
} UdpPacketHeader;

/**
 * Initialize UDP broadcast socket.
 */
void udp_broadcast_init(void);

/**
 * Send pixel data via UDP broadcast.
 * @param buffer Pixel data buffer
 * @param count Number of pixels
 * @return true if sent successfully
 */
bool udp_broadcast_send(const uint16_t* buffer, size_t count);

#ifdef __cplusplus
}
#endif

#endif // UDP_BROADCAST_H
