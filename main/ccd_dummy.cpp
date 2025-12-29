/*
 * CCD Dummy Data Generator - Implementation
 * ==========================================
 * Generates synthetic sine wave data for testing without real hardware.
 * 
 * Features:
 * - Base sine wave across pixel array
 * - Frequency modulation over time (creates "moving" effect)
 * - Amplitude modulation (pulsing intensity)
 * - Optional random noise for realism
 */

#include "ccd_dummy.h"
#include "config.h"

#include <math.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "CCD_DUMMY";

// Time tracking for animation
static uint32_t frame_count = 0;

// ============================================
// PUBLIC: Initialize dummy generator
// ============================================
void ccd_dummy_init(void) {
    frame_count = 0;
    srand((unsigned int)esp_timer_get_time());
    ESP_LOGI(TAG, "Dummy data generator initialized");
    ESP_LOGI(TAG, "  Base freq: %.1f cycles, Amplitude: %d, Noise: %d",
             DUMMY_BASE_FREQUENCY, DUMMY_AMPLITUDE, DUMMY_NOISE_LEVEL);
}

// ============================================
// PUBLIC: Generate one line of dummy data
// ============================================
size_t ccd_dummy_read_line(uint16_t* buffer) {
    // Time-based parameters (creates animation effect)
    float time_factor = (float)frame_count * 0.02f;  // Slow progression
    
    // Frequency modulation: base frequency + slow sine wave variation
    float current_freq = DUMMY_BASE_FREQUENCY + 
                        (DUMMY_BASE_FREQUENCY * DUMMY_FREQ_MODULATION * sinf(time_factor * 0.3f));
    
    // Amplitude modulation: creates pulsing effect
    float amp_mod = 0.7f + 0.3f * sinf(time_factor * 0.5f);  // 70% to 100%
    float current_amp = DUMMY_AMPLITUDE * amp_mod;
    
    // Phase shift: makes the wave "scroll" across the array
    float phase_shift = time_factor * 2.0f;
    
    // Generate sine wave with variations
    for (int i = 0; i < CCD_PIXEL_COUNT; i++) {
        // Position along the pixel array (0.0 to 1.0)
        float x = (float)i / (float)CCD_PIXEL_COUNT;
        
        // Main sine wave
        float wave = sinf(2.0f * M_PI * (current_freq * x + phase_shift));
        
        // Add secondary harmonic for more interesting shape
        wave += 0.3f * sinf(2.0f * M_PI * (current_freq * 2.0f * x + phase_shift * 1.5f));
        wave /= 1.3f;  // Normalize
        
        // Calculate pixel value
        int32_t value = DUMMY_OFFSET + (int32_t)(current_amp * wave);
        
        // Add noise
        if (DUMMY_NOISE_LEVEL > 0) {
            value += (rand() % (DUMMY_NOISE_LEVEL * 2)) - DUMMY_NOISE_LEVEL;
        }
        
        // Clamp to 12-bit range (0-4095)
        if (value < 0) value = 0;
        if (value > 4095) value = 4095;
        
        buffer[i] = (uint16_t)value;
    }
    
    // Add some "peaks" to simulate spectral features
    // Peak 1: around 25% of array
    int peak1_pos = CCD_PIXEL_COUNT / 4 + (int)(100 * sinf(time_factor * 0.7f));
    for (int i = -50; i < 50; i++) {
        int idx = peak1_pos + i;
        if (idx >= 0 && idx < CCD_PIXEL_COUNT) {
            float gaussian = expf(-(i * i) / 500.0f);
            buffer[idx] = (uint16_t)fminf(4095, buffer[idx] + 800 * gaussian);
        }
    }
    
    // Peak 2: around 60% of array
    int peak2_pos = (CCD_PIXEL_COUNT * 6) / 10 + (int)(80 * sinf(time_factor * 0.9f));
    for (int i = -30; i < 30; i++) {
        int idx = peak2_pos + i;
        if (idx >= 0 && idx < CCD_PIXEL_COUNT) {
            float gaussian = expf(-(i * i) / 200.0f);
            buffer[idx] = (uint16_t)fminf(4095, buffer[idx] + 600 * gaussian);
        }
    }
    
    frame_count++;
    
    return CCD_PIXEL_COUNT;
}
