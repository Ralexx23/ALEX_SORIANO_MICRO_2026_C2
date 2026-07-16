#include "rf_link.h"
#include "pin_config.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "rf_link";

/* ===== Parámetros del framing OOK/PWM (tipo rc-switch) =====
 * bit '0' -> pulso corto ALTO + pulso largo BAJO
 * bit '1' -> pulso largo ALTO + pulso corto BAJO
 * Preámbulo: 1 pulso ALTO largo + silencio, para sincronizar al receptor.
 * Ajusta estos tiempos según la sensibilidad de tu módulo TX/RX.
 */
#define RF_SHORT_US   300
#define RF_LONG_US    900
#define RF_SYNC_HIGH_US   300
#define RF_SYNC_LOW_US    9000

static rmt_channel_handle_t s_tx_chan;
static rmt_encoder_handle_t s_copy_encoder;
static rmt_channel_handle_t s_rx_chan;

#define RX_SYMBOL_BUF_LEN 128
static rmt_symbol_word_t s_rx_symbols[RX_SYMBOL_BUF_LEN];
static volatile int s_rx_symbol_count = 0;

static bool IRAM_ATTR rx_done_cb(rmt_channel_handle_t chan, const rmt_rx_done_event_data_t *edata, void *user_ctx)
{
    (void)chan; (void)user_ctx;
    int n = edata->num_symbols;
    if (n > RX_SYMBOL_BUF_LEN) n = RX_SYMBOL_BUF_LEN;
    memcpy((void *)s_rx_symbols, edata->received_symbols, n * sizeof(rmt_symbol_word_t));
    s_rx_symbol_count = n;
    return false;
}

esp_err_t rf_link_init(void)
{
    /* ---- TX ---- */
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = PIN_RF_TX,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1 tick = 1us
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &s_tx_chan));

    rmt_copy_encoder_config_t copy_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &s_copy_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_tx_chan));

    /* ---- RX (captura cruda del demodulador) ---- */
    rmt_rx_channel_config_t rx_cfg = {
        .gpio_num = PIN_RF_RX,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .mem_block_symbols = 128,
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_cfg, &s_rx_chan));

    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rx_done_cb };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(s_rx_chan, &cbs, NULL));
    ESP_ERROR_CHECK(rmt_enable(s_rx_chan));

    rmt_receive_config_t rx_recv_cfg = {
        .signal_range_min_ns = 50 * 1000,        // ignora ruido < 50us
        .signal_range_max_ns = 12000 * 1000,      // hasta 12ms (cubre el sync largo)
    };
    ESP_ERROR_CHECK(rmt_receive(s_rx_chan, s_rx_symbols, sizeof(s_rx_symbols), &rx_recv_cfg));

    ESP_LOGI(TAG, "rf_link listo (TX=%d, RX=%d)", PIN_RF_TX, PIN_RF_RX);
    return ESP_OK;
}

static uint8_t compute_checksum(const rf_frame_t *f)
{
    const uint8_t *b = (const uint8_t *)f;
    uint8_t x = 0;
    for (size_t i = 0; i < sizeof(rf_frame_t) - 1; i++) x ^= b[i]; // excluye el propio checksum
    return x;
}

/* Convierte cada bit de la trama en 2 símbolos RMT (alto+bajo), con
 * preámbulo de sincronización al inicio. */
static size_t encode_frame_to_symbols(const rf_frame_t *frame, rmt_symbol_word_t *out, size_t max_symbols)
{
    size_t n = 0;
    if (n + 1 > max_symbols) return n;

    /* preámbulo */
    out[n].level0 = 1; out[n].duration0 = RF_SYNC_HIGH_US;
    out[n].level1 = 0; out[n].duration1 = RF_SYNC_LOW_US;
    n++;

    const uint8_t *bytes = (const uint8_t *)frame;
    for (size_t byte_i = 0; byte_i < sizeof(rf_frame_t); byte_i++) {
        for (int bit = 7; bit >= 0; bit--) {
            if (n >= max_symbols) return n;
            bool one = (bytes[byte_i] >> bit) & 0x1;
            out[n].level0 = 1;
            out[n].duration0 = one ? RF_LONG_US : RF_SHORT_US;
            out[n].level1 = 0;
            out[n].duration1 = one ? RF_SHORT_US : RF_LONG_US;
            n++;
        }
    }
    return n;
}

esp_err_t rf_link_send_frame(rf_frame_t *frame)
{
    frame->checksum = compute_checksum(frame);

    static rmt_symbol_word_t symbols[8 + 8 * sizeof(rf_frame_t)];
    size_t n = encode_frame_to_symbols(frame, symbols, sizeof(symbols) / sizeof(symbols[0]));

    rmt_transmit_config_t tx_conf = { .loop_count = 0 };
    esp_err_t err = rmt_transmit(s_tx_chan, s_copy_encoder, symbols, n * sizeof(rmt_symbol_word_t), &tx_conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "fallo transmitiendo trama: %s", esp_err_to_name(err));
    }
    return err;
}

int rf_link_get_last_rx_symbol_count(void)
{
    int n = s_rx_symbol_count;
    /* re-arma la captura para seguir escuchando */
    rmt_receive_config_t rx_recv_cfg = {
        .signal_range_min_ns = 50 * 1000,
        .signal_range_max_ns = 12000 * 1000,
    };
    rmt_receive(s_rx_chan, s_rx_symbols, sizeof(s_rx_symbols), &rx_recv_cfg);
    return n;
}
