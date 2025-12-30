/*
 * CCD Dummy Data Generator - Header
 * ==================================
 * Generates synthetic sine wave data for testing without real hardware.
 */

#ifndef CCD_DUMMY_H
#define CCD_DUMMY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize dummy data generator.
 */
void ccd_dummy_init(void);

/**
 * Generate one line of dummy pixel data.
 * Creates a time-varying sine wave with frequency and amplitude modulation.
 * @param buffer Output buffer for pixel data (must hold CCD_PIXEL_COUNT elements)
 * @return Number of pixels generated
 */
size_t ccd_dummy_read_line(uint16_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // CCD_DUMMY_H
