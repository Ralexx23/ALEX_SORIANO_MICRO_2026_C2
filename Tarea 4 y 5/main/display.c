#include "display.h"
#include "pin_config.h"
#include "i2c_bus.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "display";
static i2c_master_dev_handle_t s_dev;
static uint8_t s_fb[OLED_WIDTH * OLED_HEIGHT / 8];

/* ===================== bajo nivel SSD1306 ===================== */

static void cmd(uint8_t c)
{
    uint8_t buf[2] = { 0x00, c };
    i2c_master_transmit(s_dev, buf, 2, 1000 / portTICK_PERIOD_MS);
}

static void flush_framebuffer(void)
{
    cmd(0x21); cmd(0);   cmd(OLED_WIDTH - 1);   // column range
    cmd(0x22); cmd(0);   cmd((OLED_HEIGHT / 8) - 1); // page range

    uint8_t chunk[1 + 32];
    for (size_t off = 0; off < sizeof(s_fb); off += 32) {
        chunk[0] = 0x40; // control byte: datos
        memcpy(&chunk[1], &s_fb[off], 32);
        i2c_master_transmit(s_dev, chunk, 33, 1000 / portTICK_PERIOD_MS);
    }
}

esp_err_t display_init(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = I2C_CLK_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_get_handle(), &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fallo agregando OLED al bus: %s", esp_err_to_name(err));
        return err;
    }

    const uint8_t init_seq[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    for (size_t i = 0; i < sizeof(init_seq); i++) cmd(init_seq[i]);

    memset(s_fb, 0, sizeof(s_fb));
    flush_framebuffer();
    ESP_LOGI(TAG, "OLED listo (addr 0x%02X)", OLED_I2C_ADDR);
    return ESP_OK;
}

/* ===================== primitivas gráficas ===================== */

static void fb_clear(void) { memset(s_fb, 0, sizeof(s_fb)); }

static void fb_set_px(int x, int y, bool on)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    size_t idx = x + (y / 8) * OLED_WIDTH;
    uint8_t mask = 1 << (y % 8);
    if (on) s_fb[idx] |= mask; else s_fb[idx] &= ~mask;
}

static void fb_line(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_set_px(x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void fb_rect(int x, int y, int w, int h)
{
    fb_line(x, y, x + w - 1, y);
    fb_line(x, y + h - 1, x + w - 1, y + h - 1);
    fb_line(x, y, x, y + h - 1);
    fb_line(x + w - 1, y, x + w - 1, y + h - 1);
}

static void fb_fill_rect(int x, int y, int w, int h)
{
    for (int yy = y; yy < y + h; yy++) fb_line(x, yy, x + w - 1, yy);
}

static void fb_circle(int cx, int cy, int r)
{
    int x = r, y = 0, err = 0;
    while (x >= y) {
        fb_set_px(cx + x, cy + y, true); fb_set_px(cx + y, cy + x, true);
        fb_set_px(cx - y, cy + x, true); fb_set_px(cx - x, cy + y, true);
        fb_set_px(cx - x, cy - y, true); fb_set_px(cx - y, cy - x, true);
        fb_set_px(cx + y, cy - x, true); fb_set_px(cx + x, cy - y, true);
        y++;
        if (err <= 0) { err += 2 * y + 1; }
        if (err > 0)  { x--; err -= 2 * x + 1; }
    }
}

/* ===================== dígitos "7 segmentos" ===================== */
/* bits: 0=top 1=top-right 2=bottom-right 3=bottom 4=bottom-left 5=top-left 6=middle */
static const uint8_t SEG[10] = {
    0b0111111, // 0
    0b0000110, // 1
    0b1011011, // 2
    0b1001111, // 3
    0b1100110, // 4
    0b1101101, // 5
    0b1111101, // 6
    0b0000111, // 7
    0b1111111, // 8
    0b1101111, // 9
};

static void draw_digit(int x, int y, int w, int h, int d)
{
    if (d < 0 || d > 9) return;
    uint8_t s = SEG[d];
    int mx = x + w, my = y + h / 2, ey = y + h;
    if (s & 0x01) fb_line(x, y, mx, y);           // top
    if (s & 0x02) fb_line(mx, y, mx, my);          // top-right
    if (s & 0x04) fb_line(mx, my, mx, ey);         // bottom-right
    if (s & 0x08) fb_line(x, ey, mx, ey);          // bottom
    if (s & 0x10) fb_line(x, my, x, ey);           // bottom-left
    if (s & 0x20) fb_line(x, y, x, my);            // top-left
    if (s & 0x40) fb_line(x, my, mx, my);          // middle
}

/* dibuja un entero con signo, dígitos de ancho dw/alto dh, alineado a
 * la izquierda en (x,y). Devuelve el ancho total usado en píxeles. */
static int draw_number(int x, int y, int dw, int dh, int value)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", value);
    int cx = x;
    for (char *p = buf; *p; p++) {
        if (*p == '-') {
            int my = y + dh / 2;
            fb_line(cx, my, cx + dw / 2, my);
            cx += dw / 2 + 2;
        } else {
            draw_digit(cx, y, dw, dh, *p - '0');
            cx += dw + 3;
        }
    }
    return cx - x;
}

/* ===================== layout de la UI ===================== */

static void draw_joy_box(int x, int y, int size, int8_t vx, int8_t vy)
{
    fb_rect(x, y, size, size);
    int cx = x + size / 2, cy = y + size / 2;
    fb_line(x, cy, x + size - 1, cy);   // eje horizontal
    fb_line(cx, y, cx, y + size - 1);   // eje vertical
    int px = cx + (vx * (size / 2 - 2)) / 100;
    int py = cy - (vy * (size / 2 - 2)) / 100; // -y en pantalla = arriba
    fb_fill_rect(px - 1, py - 1, 3, 3);
}

static void draw_tilt_indicator(int cx, int cy, int r, float roll_deg, float pitch_deg, float max_deg)
{
    fb_circle(cx, cy, r);
    fb_set_px(cx, cy, true);
    float rr = roll_deg / max_deg; if (rr > 1) rr = 1; if (rr < -1) rr = -1;
    float rp = pitch_deg / max_deg; if (rp > 1) rp = 1; if (rp < -1) rp = -1;
    int ex = cx + (int)(rr * r);
    int ey = cy - (int)(rp * r);
    fb_line(cx, cy, ex, ey);
    fb_fill_rect(ex - 1, ey - 1, 3, 3);
}

static void draw_wifi_icon(int x, int y, bool ok)
{
    /* tres arcos simples tipo "wifi"; rellenos si hay conexión */
    for (int i = 0; i < 3; i++) {
        int r = 2 + i * 2;
        if (ok || i == 0) fb_circle(x, y + 5, r);
    }
}

static void draw_mqtt_icon(int x, int y, bool ok)
{
    if (ok) fb_fill_rect(x, y, 6, 6); else fb_rect(x, y, 6, 6);
}

static void draw_cal_icon(int x, int y, bool calibrating)
{
    if (calibrating) fb_circle(x, y, 3);
    /* parpadeo simple usando el tiempo del sistema */
    if (calibrating && ((esp_timer_get_time() / 300000) % 2)) {
        fb_fill_rect(x - 2, y - 2, 4, 4);
    }
}

void display_render(const joystick_data_t *joy, const imu_data_t *imu,
                     const button_state_t *btn, bool calibrating,
                     bool wifi_ok, bool mqtt_ok)
{
    fb_clear();

    /* Fila 1: joystick izq | inclinación (giro) | joystick der */
    draw_joy_box(0, 0, 30, joy->lx, joy->ly);
    draw_tilt_indicator(64, 15, 14, imu->roll_deg, imu->pitch_deg, 35.0f);
    draw_joy_box(97, 0, 30, joy->rx, joy->ry);

    /* Fila 2: iconos de estado */
    draw_wifi_icon(4, 33, wifi_ok);
    draw_mqtt_icon(20, 33, mqtt_ok);
    draw_cal_icon(38, 38, calibrating);

    /* Fila 3: acelerómetro en centésimas de g (numérico, req. 8) */
    int ax100 = (int)(imu->accel_x_g * 100);
    int ay100 = (int)(imu->accel_y_g * 100);
    int az100 = (int)(imu->accel_z_g * 100);
    int cx = 0;
    cx += draw_number(cx, 44, 5, 10, ax100) + 4;
    cx += draw_number(cx, 44, 5, 10, ay100) + 4;
    draw_number(cx, 44, 5, 10, az100);

    /* Fila 4: 10 botones como cuadraditos: llenos = presionado */
    bool states[10] = {
        btn->trig_l1, btn->trig_l2, btn->trig_r1, btn->trig_r2,
        btn->front_a, btn->front_b, btn->front_x, btn->front_y,
        btn->top_l, btn->top_r
    };
    for (int i = 0; i < 10; i++) {
        int bx = i * 12;
        if (states[i]) fb_fill_rect(bx, 58, 8, 6);
        else fb_rect(bx, 58, 8, 6);
    }

    flush_framebuffer();
}
