#include "control_task.h"
#include "pin_config.h"
#include "i2c_bus.h"
#include "joystick.h"
#include "buttons.h"
#include "mpu6050.h"
#include "display.h"
#include "rf_link.h"
#include "wifi_mqtt.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "control";

#define LOOP_PERIOD_MS       20   // 50Hz: suficientemente rápido para un RC
#define MQTT_PUBLISH_EVERY_N 5    // publica telemetría cada 5 ciclos (~10Hz)
#define CAL_FLASH_MS         800  // cuánto se muestra el icono de "calibrando" en pantalla tras el combo

static int8_t clamp100(int v) { return (v > 100) ? 100 : (v < -100) ? -100 : v; }

static uint16_t build_button_mask(const button_state_t *b)
{
    uint16_t m = 0;
    if (b->trig_l1) m |= RF_BTN_TRIG_L1;
    if (b->trig_l2) m |= RF_BTN_TRIG_L2;
    if (b->trig_r1) m |= RF_BTN_TRIG_R1;
    if (b->trig_r2) m |= RF_BTN_TRIG_R2;
    if (b->front_a) m |= RF_BTN_FRONT_A;
    if (b->front_b) m |= RF_BTN_FRONT_B;
    if (b->front_x) m |= RF_BTN_FRONT_X;
    if (b->front_y) m |= RF_BTN_FRONT_Y;
    return m;
}

static void control_loop(void *arg)
{
    (void)arg;
    joystick_data_t joy;
    imu_data_t imu;
    button_state_t btn;
    int cycle = 0;
    int64_t cal_flash_until_us = 0;

    for (;;) {
        joystick_read(&joy);
        mpu6050_update(&imu);
        buttons_read(&btn);

        /* ---- combo de calibración: BTN_TOP_L + BTN_TOP_R por 3s (req. 4) ---- */
        if (buttons_calibration_triggered()) {
            ESP_LOGI(TAG, "ejecutando calibración de joystick + IMU...");
            joystick_calibrate_center();
            mpu6050_calibrate_zero();
            cal_flash_until_us = esp_timer_get_time() + (int64_t)CAL_FLASH_MS * 1000;
        }
        bool show_calibrating = esp_timer_get_time() < cal_flash_until_us;

        /* ---- fusión joystick + giroscopio (req. 7) ----
         * ambos aportan al comando final; se satura a +-100 igual que
         * cualquiera de las dos fuentes por separado. */
        int8_t cmd_x = clamp100((int)joy.rx + (int)imu.tilt_x);
        int8_t cmd_y = clamp100((int)joy.ry + (int)imu.tilt_y);

        /* ---- arma y transmite la trama RF ---- */
        rf_frame_t frame = {
            .joy_lx = joy.lx, .joy_ly = joy.ly,
            .joy_rx = joy.rx, .joy_ry = joy.ry,
            .cmd_x = cmd_x, .cmd_y = cmd_y,
            .buttons = build_button_mask(&btn),
        };
        rf_link_send_frame(&frame);

        /* ---- pantalla (req. 5 y 8) ---- */
        display_render(&joy, &imu, &btn, show_calibrating,
                        wifi_mqtt_is_wifi_connected(), wifi_mqtt_is_mqtt_connected());

        /* ---- telemetría MQTT (req. 6), a menor frecuencia ---- */
        if (++cycle >= MQTT_PUBLISH_EVERY_N) {
            cycle = 0;
            wifi_mqtt_publish_telemetry(&joy, &imu, &btn);
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}

void control_task_start(void)
{
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    ESP_ERROR_CHECK(i2c_bus_init());

    ESP_ERROR_CHECK(joystick_init());
    joystick_load_calibration();

    ESP_ERROR_CHECK(buttons_init());

    ESP_ERROR_CHECK(mpu6050_init());
    mpu6050_load_calibration();

    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(rf_link_init());
    ESP_ERROR_CHECK(wifi_mqtt_init());

    xTaskCreate(control_loop, "control_loop", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "control_task iniciada");
}
