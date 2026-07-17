#include "mpu6050.h"
#include "pin_config.h"
#include "i2c_bus.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include <math.h>
#include <string.h>

static const char *TAG = "mpu6050";

/* Registros MPU6050 */
#define REG_PWR_MGMT_1    0x6B
#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_ACCEL_XOUT_H  0x3B

/* Sensibilidad a fondo de escala +-2g / +-250 dps (configuración por defecto) */
#define ACCEL_SENS_LSB_PER_G   16384.0f
#define GYRO_SENS_LSB_PER_DPS  131.0f

static i2c_master_dev_handle_t s_dev;
static int64_t s_last_update_us = 0;

/* offsets de calibración (bias de reposo) */
static float s_gyro_bias_x = 0, s_gyro_bias_y = 0, s_gyro_bias_z = 0;
static float s_roll_zero = 0, s_pitch_zero = 0;
static float s_roll_est = 0, s_pitch_est = 0;

#define TILT_MAX_DEG   35.0f   // inclinación que se mapea a +-100 (ajustable a gusto)

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, 2, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t regs_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

esp_err_t mpu6050_init(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = I2C_CLK_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_get_handle(), &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fallo agregando MPU6050 al bus: %s", esp_err_to_name(err));
        return err;
    }

    reg_write(REG_PWR_MGMT_1, 0x00);     // despertar (sale de sleep mode)
    reg_write(REG_SMPLRT_DIV, 0x07);     // sample rate divider
    reg_write(REG_CONFIG, 0x03);         // DLPF ~44Hz, reduce ruido mecánico del control
    reg_write(REG_GYRO_CONFIG, 0x00);    // +-250 dps
    reg_write(REG_ACCEL_CONFIG, 0x00);   // +-2g

    s_last_update_us = esp_timer_get_time();
    ESP_LOGI(TAG, "MPU6050 listo (addr 0x%02X)", MPU6050_I2C_ADDR);
    return ESP_OK;
}

static void read_raw(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    uint8_t d[14];
    if (regs_read(REG_ACCEL_XOUT_H, d, 14) != ESP_OK) {
        *ax = *ay = *az = *gx = *gy = *gz = 0;
        return;
    }
    int16_t raw_ax = (d[0] << 8) | d[1];
    int16_t raw_ay = (d[2] << 8) | d[3];
    int16_t raw_az = (d[4] << 8) | d[5];
    /* d[6..7] = temperatura, se ignora */
    int16_t raw_gx = (d[8] << 8) | d[9];
    int16_t raw_gy = (d[10] << 8) | d[11];
    int16_t raw_gz = (d[12] << 8) | d[13];

    *ax = raw_ax / ACCEL_SENS_LSB_PER_G;
    *ay = raw_ay / ACCEL_SENS_LSB_PER_G;
    *az = raw_az / ACCEL_SENS_LSB_PER_G;
    *gx = raw_gx / GYRO_SENS_LSB_PER_DPS;
    *gy = raw_gy / GYRO_SENS_LSB_PER_DPS;
    *gz = raw_gz / GYRO_SENS_LSB_PER_DPS;
}

void mpu6050_update(imu_data_t *out)
{
    float ax, ay, az, gx, gy, gz;
    read_raw(&ax, &ay, &az, &gx, &gy, &gz);

    gx -= s_gyro_bias_x;
    gy -= s_gyro_bias_y;
    gz -= s_gyro_bias_z;

    int64_t now = esp_timer_get_time();
    float dt = (now - s_last_update_us) / 1e6f;
    if (dt <= 0 || dt > 0.5f) dt = 0.02f; // salvaguarda ante primer ciclo o overflow
    s_last_update_us = now;

    /* Ángulo a partir del acelerómetro (referencia absoluta, con ruido) */
    float accel_roll  = atan2f(ay, az) * 180.0f / (float)M_PI;
    float accel_pitch  = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / (float)M_PI;

    /* Filtro complementario: integra giroscopio (rápido, sin ruido) y
     * corrige lentamente con acelerómetro (estable a largo plazo) */
    const float alpha = 0.98f;
    s_roll_est  = alpha * (s_roll_est + gx * dt) + (1.0f - alpha) * accel_roll;
    s_pitch_est = alpha * (s_pitch_est + gy * dt) + (1.0f - alpha) * accel_pitch;

    out->accel_x_g = ax;
    out->accel_y_g = ay;
    out->accel_z_g = az;
    out->gyro_x_dps = gx;
    out->gyro_y_dps = gy;
    out->gyro_z_dps = gz;
    out->roll_deg  = s_roll_est - s_roll_zero;
    out->pitch_deg = s_pitch_est - s_pitch_zero;

    float tx = (out->roll_deg / TILT_MAX_DEG) * 100.0f;
    float ty = (out->pitch_deg / TILT_MAX_DEG) * 100.0f;
    if (tx > 100) tx = 100; if (tx < -100) tx = -100;
    if (ty > 100) ty = 100; if (ty < -100) ty = -100;
    out->tilt_x = (int8_t)tx;
    out->tilt_y = (int8_t)ty;
}

esp_err_t mpu6050_calibrate_zero(void)
{
    /* Promedia giroscopio en reposo para el bias, y fija el ángulo
     * actual como "cero" del control */
    float sum_gx = 0, sum_gy = 0, sum_gz = 0;
    const int N = 100;
    for (int i = 0; i < N; i++) {
        float ax, ay, az, gx, gy, gz;
        read_raw(&ax, &ay, &az, &gx, &gy, &gz);
        sum_gx += gx; sum_gy += gy; sum_gz += gz;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    s_gyro_bias_x = sum_gx / N;
    s_gyro_bias_y = sum_gy / N;
    s_gyro_bias_z = sum_gz / N;

    s_roll_zero = s_roll_est;
    s_pitch_zero = s_pitch_est;

    nvs_handle_t h;
    esp_err_t err = nvs_open("rc_cal", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_set_blob(h, "gyro_bias", (float[]){s_gyro_bias_x, s_gyro_bias_y, s_gyro_bias_z}, sizeof(float) * 3);
    nvs_set_blob(h, "angle_zero", (float[]){s_roll_zero, s_pitch_zero}, sizeof(float) * 2);
    err = nvs_commit(h);
    nvs_close(h);

    ESP_LOGI(TAG, "IMU calibrado: bias_gyro=(%.2f,%.2f,%.2f) zero=(%.1f,%.1f)",
             s_gyro_bias_x, s_gyro_bias_y, s_gyro_bias_z, s_roll_zero, s_pitch_zero);
    return err;
}

esp_err_t mpu6050_load_calibration(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("rc_cal", NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "sin calibración previa de IMU, usando default");
        return err;
    }
    float bias[3];
    size_t len = sizeof(bias);
    if (nvs_get_blob(h, "gyro_bias", bias, &len) == ESP_OK) {
        s_gyro_bias_x = bias[0]; s_gyro_bias_y = bias[1]; s_gyro_bias_z = bias[2];
    }
    float zero[2];
    len = sizeof(zero);
    if (nvs_get_blob(h, "angle_zero", zero, &len) == ESP_OK) {
        s_roll_zero = zero[0]; s_pitch_zero = zero[1];
    }
    nvs_close(h);
    return ESP_OK;
}
