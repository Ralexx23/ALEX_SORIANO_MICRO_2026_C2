#include "ssd1306.h"
#include "driver/i2c_master.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SSD1306";
static i2c_master_dev_handle_t dev_handle;
static uint8_t framebuffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

static void write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd}; // 0x00 = control byte, indica "comando"
    i2c_master_transmit(dev_handle, buf, sizeof(buf), -1);
}

static void write_data(const uint8_t *data, size_t len)
{
    uint8_t *buf = malloc(len + 1);
    buf[0] = 0x40; // control byte, indica "datos"
    memcpy(buf + 1, data, len);
    i2c_master_transmit(dev_handle, buf, len + 1, -1);
    free(buf);
}

void ssd1306_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_LCD_SDA,
        .scl_io_num = PIN_LCD_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = LCD_I2C_ADDR,
        .scl_speed_hz    = LCD_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    // Secuencia de inicializacion estandar para SSD1306 128x64
    write_cmd(0xAE); // display off
    write_cmd(0x20); write_cmd(0x02); // memory mode: page addressing
    write_cmd(0xB0); // page start
    write_cmd(0xC8); // COM scan direction invertida
    write_cmd(0x00); write_cmd(0x10); // columna baja/alta = 0
    write_cmd(0x40); // start line = 0
    write_cmd(0x81); write_cmd(0x7F); // contraste
    write_cmd(0xA1); // segment remap
    write_cmd(0xA6); // display normal (no invertido)
    write_cmd(0xA8); write_cmd(0x3F); // multiplex ratio = 63 (64 filas)
    write_cmd(0xA4); // resume RAM content
    write_cmd(0xD3); write_cmd(0x00); // display offset = 0
    write_cmd(0xD5); write_cmd(0x80); // clock divide
    write_cmd(0xD9); write_cmd(0xF1); // precharge
    write_cmd(0xDA); write_cmd(0x12); // com pins
    write_cmd(0xDB); write_cmd(0x40); // vcomh
    write_cmd(0x8D); write_cmd(0x14); // charge pump enable
    write_cmd(0xAF); // display on

    memset(framebuffer, 0, sizeof(framebuffer));
    ESP_LOGI(TAG, "SSD1306 inicializado");
}

void ssd1306_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

void ssd1306_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    int page = y / 8;
    int bit  = y % 8;
    int idx  = page * SSD1306_WIDTH + x;
    if (on) framebuffer[idx] |= (1 << bit);
    else    framebuffer[idx] &= ~(1 << bit);
}

void ssd1306_flush(void)
{
    for (int page = 0; page < 8; page++) {
        write_cmd(0xB0 + page);
        write_cmd(0x00);
        write_cmd(0x10);
        write_data(&framebuffer[page * SSD1306_WIDTH], SSD1306_WIDTH);
    }
}