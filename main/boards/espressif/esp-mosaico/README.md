# ESP-Mosaico

<div align="center">
    <a href="https://mosaico.espressif.com">
        <b> ESP-Mosaico Website (Docs, etc.) </b>
    </a>
    |
    <a href="./README_CN.md">
        <b> 简体中文 </b>
    </a>
</div>

## Introduction

ESP-Mosaico is an expandable smart interaction development kit from Espressif, built on the ESP32-S31, targeting applications such as magnetic expansion modules, motion sensing, square touch screens, and on-device multimedia. The device features a 480 × 480 QSPI square touch screen, audio codec and amplifier, 6-axis IMU, dual magnetometers, SPI NAND flash, and left/right module interfaces with two 2 × 10P, 2.54 mm standard-pitch connectors for expansion such as cameras.

ESP-Mosaico is available in different hardware revisions. The current XiaoZhi firmware supports ESP-Mosaico "Early Makers" (`v1.0`) and the ESP-Mosaico production version (`v1.2`), with support for OV3640 and SC101IOT camera module.

> [!NOTE]
>
> The current XiaoZhi firmware does not support hot-plugging modules yet. **To use the camera, insert the camera modules before startup.**

## Build

ESP-IDF v6.2 has not been officially released yet. Until it is, build with ESP-IDF `master` at commit `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`.

```sh
source /path/to/esp-idf-master/export.sh
python3 scripts/build.py espressif/esp-mosaico
```

When this repository is placed under an ESP-Mosaico workspace `projects/`
directory, its root CMake profile also preserves Vibe Mode, exposes ESP-Iris in
the normal application, and packages the generated XiaoZhi assets for a product
System Update. Install it from the workspace root with:

```sh
python3 mosaico.py iris system-update --project projects/xiaozhi-esp32
```

The UI needs XiaoZhi's common `sdkconfig.defaults`, the ESP32-S31 defaults, and
the Mosaico product profile. The common `LV_FONT_FMT_TXT_LARGE` setting matches
the CBIN glyph layout, and `LV_USE_LODEPNG` decodes emoji PNGs. Omitting them
leaves icons visible but text and emoji blank. Root CMake checks these settings;
regenerate older configurations using a new `SDKCONFIG` path, since adding
defaults does not override existing configuration values.
`cmake/mosaico-retained.defaults` keeps bootloader size optimization so it fits
within the existing fixed partition boundaries. It also preserves the workspace's
application debug optimization: switching to the common size optimization caused
a speech-model initialization crash on the pinned ESP32-S31 build during validation.

ESP-Iris screenshots render LVGL and encode JPEG on a separate task to avoid
exhausting the protocol task's stack. Captures use native RGB565 byte order for
both CLI screenshots and XiaoZhi's screenshot tool.

## Companion UI and interaction

The native 480 × 480 LVGL view uses soft white, brand orange and an original
companion character. Tap the character or chat button to talk. During conversation
the character shrinks to leave room for text. Long replies scroll; the history
button opens the last 20 messages, bounded to approximately 2 KB each with UTF-8
safe truncation. Returning to idle preserves history; reboot clears this RAM-only
history. Pull down the header or tap the title for volume, brightness, clearing
history and **Network setup** (网络配网). Network setup closes the menu and uses
XiaoZhi's existing hotspot provisioning flow; it does not erase saved networks.
During activation/upgrade or other unsupported busy states, the UI asks to retry
when idle.

At network startup, existing XiaoZhi Wi-Fi settings take priority. Only when its
saved network list is empty, the board reads `wifi_ssid` and `wifi_password` via
`mosaico_shared_get()` and saves a valid pair as XiaoZhi's initial network.
The Vibe Mode shared values remain read-only. An empty password supports an open
network; missing or invalid values fall back to the usual provisioning flow.
Later edits to Vibe Mode credentials do not overwrite XiaoZhi's saved networks.

The camera button opens the left CameraBoard viewfinder. The orange shutter sends
a photo for visual description; Ask returns to voice input. The vision endpoint is
provided through normal XiaoZhi MCP initialization, so connect to the voice service
once before the first inspection. Missing cameras and capture/service failures are
shown in the page. Preview runs only while idle to prioritize voice; a single
inspection remains available while preview is paused. This is not video recording.

The viewfinder targets 10 fps and presents native 432 × 288 RGB565 frames. PPA
applies the BSP left-slot rotation/mirror and an aspect-preserving center crop
(384 × 576 sensor pixels, scaled equally by 0.75). Preview borrows one camera
buffer per frame and uses two reusable, reference-counted output buffers. LVGL
holds a lease until replacement under its lock, so rendering cannot race PPA.
The original 1280 × 720 capture remains available to image inspection. Preview
fps and acquisition/conversion/presentation time are logged every five seconds;
sensor, display, and voice activity determine the achieved rate.

BMI270 provides gentle gaze movement and pickup feedback. Pickup wakes the display
and gives a short haptic pulse without activating the microphone. Pulses are limited
to 10–80 ms and stopped by an independent esp_timer, unaffected by I2C read delays.

The board owns these services until reboot. `MosaicoUi` owns only view state and
bounded history; all access holds the LVGL lock. `MosaicoDisplay` adapts fonts,
snapshots and frame lifetime. Its display invalidation callback matches the BSP's
four-pixel horizontal QSPI alignment before rendering, keeping partial-refresh
buffers consistent with the CO5300 address window. The simulator asserts aligned
flushes during settings, scrolling and camera updates. `MosaicoCamera` uses a low-priority worker and a mutex
around the entire capture/explain transaction so MCP and preview cannot overwrite
each other's frame. `MosaicoFeedback` samples on a separate task using the shared
I2C bus. UI callbacks schedule main-loop work instead of HTTP/audio lifecycle/I2C
inside LVGL callbacks.

The LCD bus follows BSP fixes `f629a8e` and `e1db301`: request eight RGB565
lines per DMA chunk (7680 bytes), disable direct PSRAM DMA, and explicitly set
SCL/DATA0–3 to `GPIO_DRIVE_CAP_DEFAULT` at 40 MHz. Keep these transport settings
in addition to the area alignment; a clean LVGL snapshot does not verify the
physical panel's transfers or rule out persistent horizontal artifacts.

The Mosaico LVGL draw buffer holds four full-width RGB565 rows (3840 bytes),
so a partial refresh fits in one SPI transaction and one DMA node. Narrow
rectangles may span more rows but keep the same pixel budget. Startup checks
this budget against `spi_bus_get_max_transaction_len()` and logs the actual
transaction capacity: the bus's requested 7680 bytes can be rounded up by IDF.
The native simulator uses the same draw-buffer budget and asserts every flush
fits. Physical panel observation is still required to validate persistent
stripe behavior and the refresh-rate impact of smaller updates.

`scripts/mosaico-ui-sim` builds the same view using the resolved LVGL component,
system SDL2, CMake, C++20 and Noto CJK fonts:

```sh
cmake -S scripts/mosaico-ui-sim -B .codex-runs/ui-sim -G Ninja
cmake --build .codex-runs/ui-sim
cd .codex-runs/ui-sim
./mosaico_ui_sim               # pointer/scroll assertions and PPM snapshots
./mosaico_ui_sim --interactive # interactive window
```

The character and auxiliary font are embedded in the application; see
`assets/README.md`. External resource partitions remain unchanged. Select updates
through the workspace CLI's full partition and product compatibility checks.
