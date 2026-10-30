#pragma once
#include <cstddef>
constexpr int MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_SPIRAM = 2;
inline size_t heap_caps_get_free_size(int) { return 100000; }
