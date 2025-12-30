/*
 * USB Serial Data Transfer - Implementation
 * ==========================================
 * Sends CCD data over USB serial (UART0) for high-speed wired connection.
 * 
 * Packet format:
 *   [0xAA] [seq_hi] [seq_lo] [count_hi] [count_lo] [pixel_data...] [0x55]
 */

#include "usb_serial.h"
#include "config.h"

#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "USB_SERIAL";
static uint16_t packet_sequence = 0;

// ============================================
// PUBLIC: Initialize USB Serial
// ============================================
void usb_serial_init(void) {
    uart_config_t uart_config = {
        .baud_rate = USB_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Configure UART0 (USB port)
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, 
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Install driver with TX buffer for non-blocking writes
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 1024 * 16, 0, NULL, 0));
    
    ESP_LOGI(TAG, "USB Serial initialized at %d baud", USB_BAUD_RATE);
}

// ============================================
// PUBLIC: Send pixel data via USB Serial
// ============================================
bool usb_serial_send(const uint16_t* buffer, size_t count) {
    // Static packet buffer
    static uint8_t packet[8 + CCD_PIXEL_COUNT * 2];
    
    // Build packet header
    size_t idx = 0;
    packet[idx++] = USB_START_MARKER;           // Start marker
    packet[idx++] = (packet_sequence >> 8) & 0xFF;   // Sequence high
    packet[idx++] = packet_sequence & 0xFF;          // Sequence low
    packet[idx++] = (count >> 8) & 0xFF;        // Count high
    packet[idx++] = count & 0xFF;               // Count low
    
    // Copy pixel data (little-endian)
    memcpy(&packet[idx], buffer, count * sizeof(uint16_t));
    idx += count * sizeof(uint16_t);
    
    // End marker
    packet[idx++] = USB_END_MARKER;
    
    // Send via UART (non-blocking with TX buffer)
    int written = uart_write_bytes(UART_NUM_0, packet, idx);
    
    packet_sequence++;
    
    // Log every 500 packets
    if (packet_sequence % 500 == 0) {
        ESP_LOGI(TAG, "Sent %u packets (%d bytes each)", packet_sequence, (int)idx);
    }
    
    return written == (int)idx;
}
