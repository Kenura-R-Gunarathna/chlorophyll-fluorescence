/*
 * TCD1304 CCD Spectrum Analyzer - Main
 * =====================================
 * 
 * Reads CCD sensor data (or dummy data for testing) and sends to PC
 * via USB Serial, WiFi AP, or WiFi Station mode.
 * 
 * Configure CONNECTION_MODE in config.h:
 *   CONNECTION_MODE_USB      - USB Serial (fastest, wired)
 *   CONNECTION_MODE_WIFI_AP  - ESP32 creates WiFi network
 *   CONNECTION_MODE_WIFI_STA - ESP32 joins your WiFi network
 */

#include "config.h"

// Connection mode includes
#if CONNECTION_MODE == CONNECTION_MODE_USB
    #include "usb_serial.h"
#elif CONNECTION_MODE == CONNECTION_MODE_WIFI_AP
    #include "wifi_ap.h"
    #include "udp_broadcast.h"
#elif CONNECTION_MODE == CONNECTION_MODE_WIFI_STA
    #include "wifi_station.h"
    #include "udp_broadcast.h"
#endif

// Data source includes
#if USE_DUMMY_DATA
    #include "ccd_dummy.h"
#else
    #include "ccd_sensor.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "MAIN";

// Pixel buffer
static uint16_t pixel_buffer[CCD_PIXEL_COUNT];

// ============================================
// CCD CAPTURE TASK
// ============================================
static void ccd_capture_task(void* pvParameters) {
    ESP_LOGI(TAG, "CCD capture task started");
    
#if USE_DUMMY_DATA
    ESP_LOGW(TAG, "Data: DUMMY (sine wave)");
#else
    ESP_LOGI(TAG, "Data: REAL CCD sensor");
#endif

#if CONNECTION_MODE == CONNECTION_MODE_USB
    ESP_LOGI(TAG, "Connection: USB Serial @ %d baud", USB_BAUD_RATE);
#elif CONNECTION_MODE == CONNECTION_MODE_WIFI_AP
    ESP_LOGI(TAG, "Connection: WiFi AP + UDP");
#elif CONNECTION_MODE == CONNECTION_MODE_WIFI_STA
    ESP_LOGI(TAG, "Connection: WiFi Station + UDP");
#endif
    
    while (1) {
        // Read data (either from real sensor or dummy generator)
#if USE_DUMMY_DATA
        size_t pixels = ccd_dummy_read_line(pixel_buffer);
#else
        size_t pixels = ccd_sensor_read_line(pixel_buffer);
#endif
        
        // Send data via configured connection
        if (pixels > 0) {
#if CONNECTION_MODE == CONNECTION_MODE_USB
            usb_serial_send(pixel_buffer, pixels);
#else
            udp_broadcast_send(pixel_buffer, pixels);
#endif
        }
        
        // Delay based on connection type
#if CONNECTION_MODE == CONNECTION_MODE_USB
        vTaskDelay(pdMS_TO_TICKS(5));   // USB can handle faster rates
#else
        vTaskDelay(pdMS_TO_TICKS(10));  // WiFi needs more time
#endif
    }
}

// ============================================
// MAIN ENTRY POINT
// ============================================
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  TCD1304 CCD Spectrum Analyzer");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
#if CONNECTION_MODE == CONNECTION_MODE_USB
    // ---- USB Serial Mode ----
    ESP_LOGI(TAG, "[1/2] Initializing USB Serial...");
    usb_serial_init();
    
    ESP_LOGI(TAG, "[2/2] Initializing data source...");
    
#elif CONNECTION_MODE == CONNECTION_MODE_WIFI_AP
    // ---- WiFi Access Point Mode ----
    ESP_LOGI(TAG, "[1/4] Initializing WiFi AP...");
    wifi_ap_init();
    
    ESP_LOGI(TAG, "[2/4] Waiting for WiFi...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "[3/4] Initializing data source...");
    
#elif CONNECTION_MODE == CONNECTION_MODE_WIFI_STA
    // ---- WiFi Station Mode ----
    ESP_LOGI(TAG, "[1/4] Initializing WiFi Station...");
    if (!wifi_station_init()) {
        ESP_LOGE(TAG, "WiFi connection failed! Check SSID/password in config.h");
        return;
    }
    
    ESP_LOGI(TAG, "[2/4] WiFi connected, IP: %s", wifi_station_get_ip());
    
    ESP_LOGI(TAG, "[3/4] Initializing data source...");
#endif

    // Initialize data source
#if USE_DUMMY_DATA
    ccd_dummy_init();
#else
    ccd_sensor_init();
#endif
    
    // Initialize network broadcast (for WiFi modes)
#if CONNECTION_MODE != CONNECTION_MODE_USB
    ESP_LOGI(TAG, "[4/4] Initializing UDP broadcast...");
    udp_broadcast_init();
#endif
    
    // Print ready message
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== READY ===");
    
#if CONNECTION_MODE == CONNECTION_MODE_USB
    ESP_LOGI(TAG, "Mode: USB Serial @ %d baud", USB_BAUD_RATE);
#elif CONNECTION_MODE == CONNECTION_MODE_WIFI_AP
    ESP_LOGI(TAG, "Mode: WiFi Access Point");
    ESP_LOGI(TAG, "Connect to: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "UDP port: %d", UDP_PORT);
#elif CONNECTION_MODE == CONNECTION_MODE_WIFI_STA
    ESP_LOGI(TAG, "Mode: WiFi Station");
    ESP_LOGI(TAG, "IP: %s", wifi_station_get_ip());
    ESP_LOGI(TAG, "UDP port: %d", UDP_PORT);
#endif

#if USE_DUMMY_DATA
    ESP_LOGW(TAG, "Data: DUMMY (change USE_DUMMY_DATA in config.h)");
#else
    ESP_LOGI(TAG, "Data: REAL CCD SENSOR");
#endif
    ESP_LOGI(TAG, "");
    
    // Start capture task
    xTaskCreate(ccd_capture_task, "ccd_capture", 8192, NULL, 5, NULL);
}
