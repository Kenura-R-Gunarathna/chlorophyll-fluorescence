/*
 * WiFi Access Point - Header
 * ==========================
 * ESP32 WiFi soft AP functionality.
 */

#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize and start WiFi Access Point.
 * Configures SSID, password, and event handlers.
 */
void wifi_ap_init(void);

/**
 * Get number of currently connected clients.
 * @return Number of connected stations
 */
uint8_t wifi_ap_get_client_count(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_AP_H
