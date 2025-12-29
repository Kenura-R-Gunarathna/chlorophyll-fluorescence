/*
 * TCD1304 CCD Spectrum Analyzer - Configuration
 * ==============================================
 * All pin definitions, WiFi settings, and feature toggles in one place.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "hal/adc_types.h"

// ============================================
// FEATURE TOGGLE - Switch between real/dummy data
// ============================================
// Set to 1 to use dummy sine wave data (for testing without hardware)
// Set to 0 to use real CCD sensor
#define USE_DUMMY_DATA  1

// ============================================
// CONNECTION MODE - How to send data to PC
// ============================================
// USB     = USB Serial (wired, fastest, most reliable)
// WIFI_AP = WiFi Access Point + UDP (ESP32 creates network)
// WIFI_STA = WiFi Station + UDP (ESP32 joins your network)
#define CONNECTION_MODE_USB       0
#define CONNECTION_MODE_WIFI_AP   1
#define CONNECTION_MODE_WIFI_STA  2

#define CONNECTION_MODE  CONNECTION_MODE_WIFI_STA

// ============================================
// USB SERIAL CONFIG (for CONNECTION_MODE_USB)
// ============================================
#define USB_BAUD_RATE       921600  // High speed serial
#define USB_START_MARKER    0xAA
#define USB_END_MARKER      0x55

// ============================================
// WiFi STATION CONFIG (for CONNECTION_MODE_WIFI_STA)
// ============================================
// Change these to your home WiFi credentials
#define WIFI_STA_SSID       "SLT-4G-187E"
#define WIFI_STA_PASSWORD   "krag#xz1"
#define WIFI_STA_MAX_RETRY  10

// ============================================
// WiFi ACCESS POINT CONFIG (for CONNECTION_MODE_WIFI_AP)
// ============================================
#define WIFI_AP_SSID        "CCD_TCD1304"
#define WIFI_AP_PASSWORD    ""          // Empty = open network
#define WIFI_AP_CHANNEL     1
#define WIFI_AP_MAX_CONN    4

// ============================================
// NETWORK CONFIG (for WiFi modes)
// ============================================
#define UDP_PORT            8080
#define UDP_BROADCAST_IP    "255.255.255.255"  // Works for both AP and STA

// ============================================
// PIN DEFINITIONS - CCD Sensor Connections
// ============================================
#define PIN_SH          GPIO_NUM_32   // Shift Gate
#define PIN_MCLK        GPIO_NUM_25   // Master Clock (800kHz PWM)
#define PIN_ICG         GPIO_NUM_33   // Integration Clear Gate
#define PIN_ADC         GPIO_NUM_36   // ADC input
#define ADC_CHANNEL     ADC_CHANNEL_0 // GPIO36 = ADC1_CH0

// ============================================
// CCD SENSOR CONFIG
// ============================================
#define CCD_SAMPLE_RATE     200000      // 200kHz ADC sampling
#define CCD_PIXEL_COUNT     3694        // TCD1304 has 3694 pixels

// ============================================
// PWM CONFIG (Master Clock)
// ============================================
#define MCLK_FREQUENCY      800000      // 800kHz
#define MCLK_DUTY_PERCENT   50          // 50% duty cycle

// ============================================
// DUMMY DATA CONFIG (for testing)
// ============================================
#define DUMMY_BASE_FREQUENCY    10.0f   // Base sine wave cycles across array
#define DUMMY_FREQ_MODULATION   0.1f    // How much frequency varies over time
#define DUMMY_AMPLITUDE         2000    // Peak amplitude (0-4095 range)
#define DUMMY_OFFSET            2048    // Center offset
#define DUMMY_NOISE_LEVEL       50      // Random noise amount

#endif // CONFIG_H
