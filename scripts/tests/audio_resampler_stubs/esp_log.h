#pragma once
#include <cstdio>
#define ESP_LOGI(tag, ...)                 \
    do {                                   \
        std::fprintf(stderr, "%s: ", tag); \
        std::fprintf(stderr, __VA_ARGS__); \
        std::fprintf(stderr, "\n");        \
    } while (0)
#define ESP_LOGE ESP_LOGI
