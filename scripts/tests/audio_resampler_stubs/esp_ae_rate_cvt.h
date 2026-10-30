#pragma once
#include <cstdint>
using esp_ae_rate_cvt_handle_t = void*;
using esp_ae_sample_t = void*;
using esp_ae_err_t = int;
constexpr int ESP_AE_ERR_OK = 0;
constexpr int ESP_AE_RATE_CVT_PERF_TYPE_SPEED = 1;
struct esp_ae_rate_cvt_cfg_t {
    uint32_t src_rate, dest_rate;
    uint8_t channel, bits_per_sample, complexity;
    int perf_type;
};
int esp_ae_rate_cvt_open(esp_ae_rate_cvt_cfg_t*, void**);
void esp_ae_rate_cvt_close(void*);
int esp_ae_rate_cvt_reset(void*);
int esp_ae_rate_cvt_get_max_out_sample_num(void*, uint32_t, uint32_t*);
int esp_ae_rate_cvt_process(void*, void*, uint32_t, void*, uint32_t*);
