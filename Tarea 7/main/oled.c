#include "oled.h"
#include "config.h"
#include "u8g2.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "OLED";

static u8g2_t u8g2;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t oled_dev = NULL;

#define U8X8_BUF_LEN 32
static uint8_t tx_buffer[U8X8_BUF_LEN];
static uint8_t tx_buf_idx = 0;

/* ---------------- Callback de bytes I2C ---------------- */
static uint8_t u8x8_byte_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_SET_DC:
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            tx_buf_idx = 0;
            break;
        case U8X8_MSG_BYTE_SEND: {
            uint8_t *data = (uint8_t *)arg_ptr;
            for (uint8_t i = 0; i < arg_int; i++) {
                if (tx_buf_idx < U8X8_BUF_LEN) {
                    tx_buffer[tx_buf_idx++] = data[i];
                }
            }
            break;
        }
        case U8X8_MSG_BYTE_END_TRANSFER: {
            esp_err_t err = i2c_master_transmit(oled_dev, tx_buffer, tx_buf_idx, 100);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "I2C transmit fallo: %s", esp_err_to_name(err));
            }
            break;
        }
        default:
            return 0;
    }
    return 1;
}

/* ---------------- Callback GPIO/Delay ---------------- */
static uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;
        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us(10);
            break;
        case U8X8_MSG_DELAY_100NANO:
            esp_rom_delay_us(1);
            break;
        case U8X8_MSG_GPIO_RESET:
            break;
        default:
            return 0;
    }
    return 1;
}

static void i2c_bus_init(void) {
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_config, &oled_dev));
}

void oled_init(void) {
    i2c_bus_init();

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2, U8G2_R0, u8x8_byte_i2c, u8x8_gpio_and_delay);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);

    ESP_LOGI(TAG, "OLED inicializado (SDA=%d SCL=%d addr=0x%02X)",
             PIN_OLED_SDA, PIN_OLED_SCL, OLED_I2C_ADDR);
}

/* ---------------- Helpers de pantalla ---------------- */
static void draw_title_and_line(const char *title, const char *line1, const char *line2) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_7x14B_tr);
    u8g2_DrawStr(&u8g2, 0, 14, title);
    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
    if (line1) u8g2_DrawStr(&u8g2, 0, 34, line1);
    if (line2) u8g2_DrawStr(&u8g2, 0, 50, line2);
    u8g2_SendBuffer(&u8g2);
}

void oled_show_mode_idle(game_mode_t mode) {
    if (mode == GAME_MODE_REACTION) {
        draw_title_and_line("REACTION", "Manten PB1", "para iniciar");
    } else {
        draw_title_and_line("MASH TEST", "Presiona BOOT", "para volver");
    }
}

void oled_show_wait_hold(void) {
    draw_title_and_line("REACTION", "Manten PB1...", NULL);
}

void oled_show_get_ready(void) {
    draw_title_and_line("REACTION", "Preparate...", NULL);
}

void oled_show_react_now(void) {
    draw_title_and_line("REACTION", "SUELTA!", "ahora");
}

void oled_show_wait_second(void) {
    draw_title_and_line("REACTION", "Presiona PB2", NULL);
}

void oled_show_result_m1(result_m1_t result) {
    if (result.fault) {
        draw_title_and_line("RESULTADO", "FALLO:", "soltaste muy pronto");
        return;
    }
    char line1[32];
    char line2[32];
    snprintf(line1, sizeof(line1), "Reaccion: %ld ms", (long)result.reaction_ms);
    snprintf(line2, sizeof(line2), "2do bot: %ld ms", (long)result.second_press_ms);
    draw_title_and_line("RESULTADO", line1, line2);
}

void oled_show_fault(void) {
    draw_title_and_line("FALLO", "Soltaste muy", "pronto! Reintenta");
}

void oled_show_countdown_m2(int seconds_left) {
    char line1[16];
    snprintf(line1, sizeof(line1), "%d...", seconds_left);
    draw_title_and_line("MASH TEST", "Preparate", line1);
}

void oled_show_mash_running(int32_t count) {
    char line1[24];
    snprintf(line1, sizeof(line1), "Presiones: %ld", (long)count);
    draw_title_and_line("MASH!", line1, NULL);
}

void oled_show_mash_error(int32_t count) {
    char line1[24];
    snprintf(line1, sizeof(line1), "Presiones: %ld", (long)count);
    draw_title_and_line("MASH!", line1, "Error: alternar!");
}

void oled_show_result_m2(result_m2_t result) {
    char line1[32];
    char line2[32];
    snprintf(line1, sizeof(line1), "Total: %ld", (long)result.total_presses);
    snprintf(line2, sizeof(line2), "Prom: %ld ms", (long)result.avg_interval_ms);
    draw_title_and_line("RESULTADO", line1, line2);
}