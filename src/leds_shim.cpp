// Driver WS2812 sobre o RMT5 (driver_ng) do ESP-IDF 5.
// 4 LEDs endereçáveis, cor sólida por LED, ordem GRB (padrão do WS2812B).
// Resolução 10 MHz => 1 tick = 100 ns; WS2812 pede ~1,25 µs por bit.

#include "leds_shim.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LED_RES_HZ     10000000
#define LED_T0H_TICKS  4        // 0,40 µs
#define LED_T0L_TICKS  8        // 0,80 µs
#define LED_T1H_TICKS  8        // 0,80 µs
#define LED_T1L_TICKS  4        // 0,40 µs
#define LED_MAX_LEDS   32

FastLEDShim FastLED;

static rmt_channel_handle_t s_chan = nullptr;
static rmt_encoder_handle_t s_enc = nullptr;
static int s_pin = -1;
static rmt_symbol_word_t s_symbols[LED_MAX_LEDS * 24];
static SemaphoreHandle_t s_mutex = nullptr;

static bool led_channel_init(int pin)
{
    if (s_chan && s_pin == pin) return true;

    if (s_chan) {
        rmt_disable(s_chan);
        rmt_del_channel(s_chan);
        s_chan = nullptr;
    }

    rmt_tx_channel_config_t cfg = {};
    cfg.gpio_num          = (gpio_num_t) pin;
    cfg.clk_src           = RMT_CLK_SRC_DEFAULT;
    cfg.resolution_hz     = LED_RES_HZ;
    cfg.mem_block_symbols = 64;
    cfg.trans_queue_depth = 1;
    if (rmt_new_tx_channel(&cfg, &s_chan) != ESP_OK) return false;

    if (!s_enc) {
        rmt_copy_encoder_config_t enc_cfg = {};
        if (rmt_new_copy_encoder(&enc_cfg, &s_enc) != ESP_OK) return false;
    }

    if (rmt_enable(s_chan) != ESP_OK) return false;
    s_pin = pin;
    return true;
}

void FastLEDShim::show()
{
    if (!_data || _count <= 0 || _pin < 0) return;
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) != pdTRUE) return;
    if (_count > LED_MAX_LEDS) _count = LED_MAX_LEDS;
    if (!led_channel_init(_pin)) {
        ESP_LOGE("leds", "falha ao iniciar canal RMT do WS2812");
        return;
    }

    int n = 0;
    for (int i = 0; i < _count; i++) {
        uint8_t grb[3];
        grb[0] = (uint8_t) ((uint16_t) _data[i].g * _brightness / 255);
        grb[1] = (uint8_t) ((uint16_t) _data[i].r * _brightness / 255);
        grb[2] = (uint8_t) ((uint16_t) _data[i].b * _brightness / 255);

        for (int byte = 0; byte < 3; byte++) {
            for (int bit = 7; bit >= 0; bit--) {
                bool one = (grb[byte] >> bit) & 0x01;
                s_symbols[n].level0    = 1;
                s_symbols[n].duration0 = one ? LED_T1H_TICKS : LED_T0H_TICKS;
                s_symbols[n].level1    = 0;
                s_symbols[n].duration1 = one ? LED_T1L_TICKS : LED_T0L_TICKS;
                n++;
            }
        }
    }

    rmt_transmit_config_t tc = {};
    tc.loop_count = 0;
    if (rmt_transmit(s_chan, s_enc, s_symbols, n * sizeof(rmt_symbol_word_t), &tc) == ESP_OK) {
        rmt_tx_wait_all_done(s_chan, 1000);
    }
    if (s_mutex) xSemaphoreGive(s_mutex);
}
