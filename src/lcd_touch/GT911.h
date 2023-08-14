/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "../ESP_PanelLcdTouch.h"

class ESP_PanelLcdTouch_GT911 : public ESP_PanelLcdTouch {
public:
    ESP_PanelLcdTouch_GT911(ESP_PanelBus *bus, const esp_lcd_touch_config_t *config): ESP_PanelLcdTouch(bus, config) { };
    ESP_PanelLcdTouch_GT911(ESP_PanelBus *bus, uint16_t width, uint16_t height): ESP_PanelLcdTouch(bus, width, height) { };
    ~ESP_PanelLcdTouch_GT911() override;

    void begin(void) override;
};

/**
 * @brief I2C address of the GT911 controller
 *
 */
#define ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS (0x5D)

/**
 * @brief Touch IO configuration structure
 *
 */
#define ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG()           \
    {                                       \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, \
        .control_phase_bytes = 1,           \
        .dc_bit_offset = 0,                 \
        .lcd_cmd_bits = 16,                 \
        .flags =                            \
        {                                   \
            .disable_control_phase = 1,     \
        }                                   \
    }
