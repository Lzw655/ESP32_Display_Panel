/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ESP_PANELBUS_SPI_H
#define ESP_PANELBUS_SPI_H

#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"

#include "../ESP_PanelBus.h"

#ifdef __cplusplus
#define need_redefine_cplusplus __cplusplus
#undef __cplusplus
#endif
#include "hal/spi_ll.h"
#ifdef need_redefine_cplusplus
#define __cplusplus need_redefine_cplusplus
#endif

#define SPI_HOST_ID_DEFAULT         (SPI2_HOST)
#define SPI_HOST_CONFIG_DEFAULT(clk, mosi, miso)                \
    {                                                           \
        .mosi_io_num = mosi,                                    \
        .miso_io_num = miso,                                    \
        .sclk_io_num = clk,                                     \
        .quadwp_io_num = GPIO_NUM_NC,                           \
        .quadhd_io_num = GPIO_NUM_NC,                           \
        .max_transfer_sz = SPI_LL_DATA_MAX_BIT_LEN >> 3,        \
    }
#define SPI_PANEL_IO_CONFIG_DEFAULT(cs, dc)                     \
    {                                                           \
        .cs_gpio_num = cs,                                      \
        .dc_gpio_num = dc,                                      \
        .spi_mode = 0,                                          \
        .pclk_hz = SPI_MASTER_FREQ_40M,                         \
        .trans_queue_depth = 10,                                \
        .on_color_trans_done = (esp_lcd_panel_io_color_trans_done_cb_t)callback, \
        .user_ctx = &this->ctx,                                 \
        .lcd_cmd_bits = 8,                                      \
        .lcd_param_bits = 8,                                    \
    }

class ESP_PanelBus_SPI: public ESP_PanelBus {
public:
    ESP_PanelBus_SPI(const esp_lcd_panel_io_spi_config_t *io_config, const spi_bus_config_t *bus_config, spi_host_device_t host_id = SPI_HOST_ID_DEFAULT);
    ESP_PanelBus_SPI(const esp_lcd_panel_io_spi_config_t *io_config, spi_host_device_t host_id = SPI_HOST_ID_DEFAULT);
    ESP_PanelBus_SPI(int cs, int dc, int clk, int mosi, int miso = -1);
    ESP_PanelBus_SPI(int cs, int dc);
    ~ESP_PanelBus_SPI() override;

    void init(void) override;

private:
    spi_host_device_t host_id;
    spi_bus_config_t host_config;
    esp_lcd_panel_io_spi_config_t io_config;
};

#endif
