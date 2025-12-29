/*
 * WiFi Station Mode - Header
 * ==========================
 * Connects ESP32 to an existing WiFi network as a client.
 */

#ifndef WIFI_STATION_H
#define WIFI_STATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize WiFi in station mode (connect to existing network).
 * Blocks until connected or max retries reached.
 * @return true if connected successfully
 */
bool wifi_station_init(void);

/**
 * Get the assigned IP address as string.
 * @return IP address string (valid after wifi_station_init returns true)
 */
const char* wifi_station_get_ip(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STATION_H
