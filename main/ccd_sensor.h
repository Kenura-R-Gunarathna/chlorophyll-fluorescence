/*
 * CCD Sensor Driver - Header
 * ==========================
 * Real TCD1304 CCD sensor control using I2S DMA for fast ADC sampling.
 */

#ifndef CCD_SENSOR_H
#define CCD_SENSOR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize CCD sensor hardware.
 * Sets up GPIO pins, PWM for MCLK, and I2S for ADC DMA.
 */
void ccd_sensor_init(void);

/**
 * Read one line (frame) from the CCD sensor.
 * @param buffer Output buffer for pixel data (must hold CCD_PIXEL_COUNT elements)
 * @return Number of pixels read, or 0 on error
 */
size_t ccd_sensor_read_line(uint16_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // CCD_SENSOR_H
