/*
 * USB Serial Data Transfer - Implementation
 * ==========================================
 * Sends CCD data over USB serial (UART0) for high-speed wired connection.
 * 
 * Partner's Protocol (compatible with Processing code):
 *   [0x11]                           - Frame start indicator
 *   [0xA5][lowByte][highByte][0x5A]  - Per-pixel packet (×3694)
 * 
 * Baud rate: 1,000,000 (1 Mbps)
 */

#include "usb_serial.h"
#include "config.h"

#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "USB_SERIAL";
static uint32_t frame_count = 0;

// ============================================
// PUBLIC: Initialize USB Serial
// ============================================
void usb_serial_init(void) {
    uart_config_t uart_config = {
        .baud_rate = USB_BAUD_RATE,  // 1,000,000 baud
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
    
    // Install driver with large TX buffer for non-blocking writes
    // Need enough for: 1 frame byte + (4 bytes × 3694 pixels) = ~14.8 KB
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 1024 * 20, 0, NULL, 0));
    
    ESP_LOGI(TAG, "USB Serial initialized at %d baud (Partner's protocol)", USB_BAUD_RATE);
    ESP_LOGI(TAG, "Protocol: 0x11=frame, 0xA5+data+0x5A=pixel");
}

// ============================================
// PUBLIC: Send pixel data via USB Serial
// ============================================
bool usb_serial_send(const uint16_t* buffer, size_t count) {
    // Send frame start indicator
    uint8_t frame_start = USB_FRAME_START;  // 0x11
    uart_write_bytes(UART_NUM_0, &frame_start, 1);
    
    // Send each pixel as: [0xA5][lowByte][highByte][0x5A]
    uint8_t pixel_packet[4];
    pixel_packet[0] = USB_PIXEL_START;  // 0xA5
    pixel_packet[3] = USB_PIXEL_END;    // 0x5A
    
    for (size_t i = 0; i < count; i++) {
        uint16_t value = buffer[i];
        pixel_packet[1] = value & 0xFF;         // Low byte
        pixel_packet[2] = (value >> 8) & 0xFF;  // High byte
        
        uart_write_bytes(UART_NUM_0, pixel_packet, 4);
    }
    
    frame_count++;
    
    // Log every 100 frames
    if (frame_count % 100 == 0) {
        ESP_LOGI(TAG, "Sent %lu frames (%d pixels each)", frame_count, (int)count);
    }
    
    return true;
}
