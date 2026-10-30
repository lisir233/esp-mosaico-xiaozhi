# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(MOSAICO_UTILS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../submodule/esp-mosaico-utils")
set(MOSAICO_BSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../submodule/esp-mosaico-bsp")
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()
# Keep XiaoZhi's common and target defaults when building directly through ESP-IDF, then
# make this checkout bootable and updateable as an ESP-Mosaico application.
# scripts/build.py can supply relative defaults; normalize them before checking
# for the base profile. Explicit defaults disable ESP-IDF's implicit base file.
set(_xiaozhi_defaults "")
foreach(_defaults IN LISTS SDKCONFIG_DEFAULTS)
    get_filename_component(_defaults "${_defaults}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    list(APPEND _xiaozhi_defaults "${_defaults}")
endforeach()
set(SDKCONFIG_DEFAULTS "${_xiaozhi_defaults}")
if(NOT "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults" IN_LIST SDKCONFIG_DEFAULTS)
    list(PREPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
endif()
if(NOT "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.esp32s31" IN_LIST SDKCONFIG_DEFAULTS)
    list(APPEND SDKCONFIG_DEFAULTS
        "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.esp32s31")
endif()
list(APPEND SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.mosaico.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/cmake/mosaico-retained.defaults")
list(REMOVE_DUPLICATES SDKCONFIG_DEFAULTS)
list(APPEND EXTRA_COMPONENT_DIRS
    "${MOSAICO_UTILS_ROOT}/esp-mosaico-vibe-mode/components/esp_mosaico_app_recovery")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include("${MOSAICO_UTILS_ROOT}/esp-mosaico-vibe-mode/cmake/mosaico_idf_project.cmake")
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "2.5.3")
project(xiaozhi)
if(CONFIG_BOARD_TYPE_ESP_MOSAICO AND
   (NOT CONFIG_LV_FONT_FMT_TXT_LARGE OR NOT CONFIG_LV_USE_LODEPNG))
    message(FATAL_ERROR
        "XiaoZhi UI assets require LV_FONT_FMT_TXT_LARGE and LV_USE_LODEPNG. "
        "The cached sdkconfig may predate the common defaults; configure with "
        "a new SDKCONFIG path to regenerate it from the complete defaults chain.")
endif()
# ESP-IDF's project() wrapper consumes PROJECT_VER for the application image,
# but does not populate CMake's PROJECT_VERSION variable used by the product
# System Update bundle.
set(PROJECT_VERSION "${PROJECT_VER}")
include("${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/system_update.cmake")
