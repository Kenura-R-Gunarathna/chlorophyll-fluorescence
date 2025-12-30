/*
 * CCD Sensor Driver - Implementation
 * ===================================
 * Real TCD1304 CCD sensor control using I2S DMA for fast ADC sampling.
 */

#include "ccd_sensor.h"
#include "config.h"

#include "driver/ledc.h"
#include "esp_adc/adc_continuous.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "CCD_SENSOR";
static adc_continuous_handle_t adc_handle = NULL;

// ============================================
// NANOSECOND DELAY
// ============================================
static inline void delay_ns(int count) {
    for (int i = 0; i < count; i++) {
        __asm__ __volatile__("nop");  // ~4.17ns per NOP at 240MHz
    }
}

// ============================================
// PWM INITIALIZATION (Master Clock)
// ============================================
static void init_mclk(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_6_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = MCLK_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK,
        .deconfigure      = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num       = PIN_MCLK,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 32,  // 50% of 64 (6-bit resolution)
        .hpoint         = 0,
        .sleep_mode     = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags          = { .output_invert = 0 }
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
    
    ESP_LOGI(TAG, "MCLK initialized at %d Hz", MCLK_FREQUENCY);
}

// ============================================
// I2S ADC INITIALIZATION
// ============================================
// ============================================
// ADC CONTINUOUS INITIALIZATION
// ============================================
static void init_adc_continuous(void) {
    // Need to read 3694 pixels × 2 bytes = 7388 bytes per frame
    // Use larger buffers to accommodate full CCD line
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024 * 16,    // 16KB buffer pool (was 4KB)
        .conv_frame_size = 1024 * 8,        // 8KB per read (was 1KB)
        .flags = {
            .flush_pool = false,
        },
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = (uint8_t)ADC_CHANNEL,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };

    adc_continuous_config_t dig_cfg = {
        .pattern_num = 1,
        .adc_pattern = &adc_pattern,
        .sample_freq_hz = CCD_SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    ESP_LOGI(TAG, "ADC Continuous initialized at %d Hz", CCD_SAMPLE_RATE);
}

// ============================================
// GPIO INITIALIZATION
// ============================================
static void init_gpio(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_SH) | (1ULL << PIN_ICG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    gpio_set_level(PIN_SH, 0);
    gpio_set_level(PIN_ICG, 1);
    
    ESP_LOGI(TAG, "GPIO initialized (SH=%d, ICG=%d)", PIN_SH, PIN_ICG);
}

// ============================================
// PUBLIC: Initialize CCD Sensor
// ============================================
void ccd_sensor_init(void) {
    ESP_LOGI(TAG, "Initializing CCD sensor hardware...");
    init_gpio();
    init_mclk();
    init_adc_continuous();
    ESP_LOGI(TAG, "CCD sensor ready!");
}

// ============================================
// PUBLIC: Read one line from CCD
// ============================================
size_t ccd_sensor_read_line(uint16_t* buffer) {
    // TCD1304 timing sequence
    gpio_set_level(PIN_ICG, 0);
    delay_ns(9);   // ~37ns
    
    gpio_set_level(PIN_SH, 1);
    delay_ns(96);  // ~400ns
    
    gpio_set_level(PIN_SH, 0);
    delay_ns(48);  // ~200ns
    
    gpio_set_level(PIN_ICG, 1);
    
    // Sample via I2S DMA
    // Note: adc_continuous_read reads bytes. With TYPE1 format, each sample is 2 bytes (16 bits) ???
    // Actually TYPE1 on ESP32 returns 4 bytes per sample? 
    // ESP32 ADC Digtal Controller Pattern Table:
    // TYPE1: 16 bits [15:12] channel, [11:0] data.
    // So it fits in uint16_t.
    
    uint32_t bytes_to_read = CCD_PIXEL_COUNT * sizeof(uint16_t); // raw data size
    uint32_t bytes_read = 0;
    
    // Start continuous ADC
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    
    // Read data (blocking)
    esp_err_t ret = adc_continuous_read(adc_handle, (uint8_t*)buffer, bytes_to_read, &bytes_read, 100);
    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
         ESP_LOGE(TAG, "ADC read error: %s", esp_err_to_name(ret));
    }
    
    // Stop continuous ADC
    ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
    
    // Invert values (CCD outputs inverted signal)
    // Process raw data (TYPE1: channel [15:12], data [11:0])
    size_t pixels_read = bytes_read / sizeof(uint16_t);
    for (size_t i = 0; i < pixels_read; i++) {
        // Mask out channel info to get 12-bit data
        uint16_t raw_val = buffer[i] & 0x0FFF;
        // Invert (CCD signal logic)
        buffer[i] = 4095 - raw_val;
    }
    
    return pixels_read;
}
