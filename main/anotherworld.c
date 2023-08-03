/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

// This file contains a simple Hello World app which you can base you own
// native Badge apps on.

#include "main.h"
#include "system_wrapper.h"
#include "filesystems.h"
#include "fpga_download.h"
#include "fpga_util.h"

static pax_buf_t buf;
xQueueHandle buttonQueue;

const char* fatal_error_str = "A fatal error occured";
const char* reset_board_str = "Reset the board to try again";

#include <esp_log.h>
static const char *TAG = "mch2022-demo-app";

// Updates the screen with the latest buffer.
void disp_flush() {
    ili9341_write(get_ili9341(), buf.buf);
}

// Exits the app, returning to the launcher.
void exit_to_launcher() {
    REG_WRITE(RTC_CNTL_STORE0_REG, 0);
    esp_restart();
}

void display_fatal_error(const char* line0, const char* line1, const char* line2, const char* line3) {
    pax_buf_t*        pax_buffer = get_pax_buffer();
    const pax_font_t* font       = pax_font_saira_regular;
    pax_noclip(pax_buffer);
    pax_background(pax_buffer, 0xa85a32);
    if (line0 != NULL) pax_draw_text(pax_buffer, 0xFFFFFFFF, font, 23, 0, 20 * 0, line0);
    if (line1 != NULL) pax_draw_text(pax_buffer, 0xFFFFFFFF, font, 18, 0, 20 * 1, line1);
    if (line2 != NULL) pax_draw_text(pax_buffer, 0xFFFFFFFF, font, 18, 0, 20 * 2, line2);
    if (line3 != NULL) pax_draw_text(pax_buffer, 0xFFFFFFFF, font, 18, 0, 20 * 3, line3);
    display_flush();
}

static void start_fpga_app(xQueueHandle button_queue, const char* path, fpga_host_fn_t fpga_host_fn) {
    ILI9341*          ili9341    = get_ili9341();
    pax_buf_t*        pax_buffer = get_pax_buffer();
    const pax_font_t* font       = pax_font_saira_regular;
    char              filename[128];
    snprintf(filename, sizeof(filename), "%s/bitstream.bin", path);
    FILE* fd = fopen(filename, "rb");
    if (fd == NULL) {
        pax_background(pax_buffer, 0xFFFFFF);
        pax_draw_text(pax_buffer, 0xFFFF0000, font, 18, 0, 0, "Failed to open file\n\nPress A or B to go back");
        display_flush();
        wait_for_button();
        return;
    }
    size_t   bitstream_length = get_file_size(fd);
    uint8_t* bitstream        = load_file_to_ram(fd);
    ICE40*   ice40            = get_ice40();
    ili9341_deinit(ili9341);
    ili9341_select(ili9341, false);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    ili9341_select(ili9341, true);
    esp_err_t res = ice40_load_bitstream(ice40, bitstream, bitstream_length);
    free(bitstream);
    fclose(fd);
    if (res == ESP_OK) {
        fpga_irq_setup(ice40);
        fpga_host_fn(button_queue, ice40, false, path);
        fpga_irq_cleanup(ice40);
        ice40_disable(ice40);
        ili9341_init(ili9341);
    } else {
        ice40_disable(ice40);
        ili9341_init(ili9341);
        pax_background(pax_buffer, 0xFFFFFF);
        pax_draw_text(pax_buffer, 0xFFFF0000, font, 18, 0, 0, "Failed to load bitstream\n\nPress A or B to go back");
        display_flush();
        wait_for_button();
    }
}

void app_main() {
  
	esp_err_t res;    

    ESP_LOGI(TAG, "Welcome to the Another Word launcher!");

    // Initialize the screen, the I2C and the SPI busses.
    bsp_init();

    // Initialize the RP2040 (responsible for buttons, etc).
    bsp_rp2040_init();
    
    // This queue is used to receive button presses.
    buttonQueue = get_rp2040()->queue;
    
    // Initialize graphics for the screen.
    pax_buf_init(&buf, NULL, 320, 240, PAX_BUF_16_565RGB);
    
    // Initialize NVS.
    nvs_flash_init();
    
    // Initialize WiFi. This doesn't connect to Wifi yet.
    wifi_init();

    /* Start FPGA driver */

    if (bsp_ice40_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize the ICE40 FPGA");
        display_fatal_error(fatal_error_str, "A hardware failure occured", "while initializing the FPGA", reset_board_str);
    }

    /* Start internal filesystem */
    if (mount_internal_filesystem() != ESP_OK) {
        display_fatal_error(fatal_error_str, "Failed to initialize flash FS", "Flash may be corrupted", reset_board_str);
    }

    /* Start SD card filesystem */
    //gpio_set_level(GPIO_SD_PWR, 1);  // Enable power to LEDs and SD card
    bool sdcard_mounted = (mount_sdcard_filesystem() == ESP_OK);
    if (sdcard_mounted) {
        ESP_LOGI(TAG, "SD card filesystem mounted");
    }

	start_fpga_app(buttonQueue, "/sd/apps/ice40/awlvl1", my_fpga_host);
	start_fpga_app(buttonQueue, "/sd/apps/ice40/aw", fpga_host);
}
