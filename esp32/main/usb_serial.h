/*
 * USB Serial Data Transfer - Header
 * ==================================
 * Sends CCD data over USB serial (UART0) for high-speed wired connection.
 */

#ifndef USB_SERIAL_H
#define USB_SERIAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize USB serial for high-speed data transfer.
 */
void usb_serial_init(void);

/**
 * Send pixel data via USB serial.
 * @param buffer Pixel data buffer
 * @param count Number of pixels
 * @return true if sent successfully
 */
bool usb_serial_send(const uint16_t* buffer, size_t count);

#ifdef __cplusplus
}
#endif

#endif // USB_SERIAL_H
