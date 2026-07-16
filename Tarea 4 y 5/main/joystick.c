#include "joystick.h"
#include "pin_config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "joystick";

static adc_oneshot_unit_handle_t s_adc1;

/* Centro de cada eje (raw ADC, 0-4095) tras calibración de usuario */
typedef struct { int lx, ly, rx, ry; } joy_center_t;
static joy_center_t s_center = { 2048, 2048, 2048, 2048 }; // default: mitad de escala

#define DEADZONE_PERCENT 4   // % alrededor del centro que se reporta como 0
#define ADC_MAX_RAW      4095

static void adc_setup_channel(adc_channel_t ch)
{
    adc_oneshot_chan_cfg_t cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, // rango completo ~0-3.3V, típico para joystick resistivo
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, ch, &cfg));
}

esp_err_t joystick_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &s_adc1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fallo creando unidad ADC1: %s", esp_err_to_name(err));
        return err;
    }

    adc_setup_channel(PIN_ADC_JOY_LX_CH);
    adc_setup_channel(PIN_ADC_JOY_LY_CH);
    adc_setup_channel(PIN_ADC_JOY_RX_CH);
    adc_setup_channel(PIN_ADC_JOY_RY_CH);

    ESP_LOGI(TAG, "joystick ADC listo");
    return ESP_OK;
}

static int8_t map_axis(int raw, int center)
{
    int diff = raw - center;
    int span = (diff >= 0) ? (ADC_MAX_RAW - center) : center;
    if (span <= 0) span = 1;

    int percent = (diff * 100) / span; // -100..+100 aprox
    if (percent > 100) percent = 100;
    if (percent < -100) percent = -100;

    if (percent > -DEADZONE_PERCENT && percent < DEADZONE_PERCENT) {
        percent = 0;
    }
    return (int8_t)percent;
}

void joystick_read(joystick_data_t *out)
{
    int raw_lx, raw_ly, raw_rx, raw_ry;
    adc_oneshot_read(s_adc1, PIN_ADC_JOY_LX_CH, &raw_lx);
    adc_oneshot_read(s_adc1, PIN_ADC_JOY_LY_CH, &raw_ly);
    adc_oneshot_read(s_adc1, PIN_ADC_JOY_RX_CH, &raw_rx);
    adc_oneshot_read(s_adc1, PIN_ADC_JOY_RY_CH, &raw_ry);

    out->lx = map_axis(raw_lx, s_center.lx);
    out->ly = map_axis(raw_ly, s_center.ly);
    out->rx = map_axis(raw_rx, s_center.rx);
    out->ry = map_axis(raw_ry, s_center.ry);
}

esp_err_t joystick_calibrate_center(void)
{
    /* Promedia varias muestras para reducir ruido al fijar el centro */
    long sum_lx = 0, sum_ly = 0, sum_rx = 0, sum_ry = 0;
    const int N = 32;
    for (int i = 0; i < N; i++) {
        int v;
        adc_oneshot_read(s_adc1, PIN_ADC_JOY_LX_CH, &v); sum_lx += v;
        adc_oneshot_read(s_adc1, PIN_ADC_JOY_LY_CH, &v); sum_ly += v;
        adc_oneshot_read(s_adc1, PIN_ADC_JOY_RX_CH, &v); sum_rx += v;
        adc_oneshot_read(s_adc1, PIN_ADC_JOY_RY_CH, &v); sum_ry += v;
    }
    s_center.lx = sum_lx / N;
    s_center.ly = sum_ly / N;
    s_center.rx = sum_rx / N;
    s_center.ry = sum_ry / N;

    nvs_handle_t h;
    esp_err_t err = nvs_open("rc_cal", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_set_blob(h, "joy_center", &s_center, sizeof(s_center));
    err = nvs_commit(h);
    nvs_close(h);

    ESP_LOGI(TAG, "joystick calibrado: LX=%d LY=%d RX=%d RY=%d",
             s_center.lx, s_center.ly, s_center.rx, s_center.ry);
    return err;
}

esp_err_t joystick_load_calibration(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("rc_cal", NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "sin calibración previa de joystick, usando default");
        return err;
    }
    size_t len = sizeof(s_center);
    err = nvs_get_blob(h, "joy_center", &s_center, &len);
    nvs_close(h);
    return err;
}
