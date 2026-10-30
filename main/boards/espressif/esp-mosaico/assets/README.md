# Companion assets

The original companion body was authored as SVG for the approved Mosaico-style
UI. It is not Mosaico's official mascot. Brand color references:
https://mosaico.ai/en (`#F7F6F9`, `#FF4C01`, `#38393A`). No website images are used.

`companion-body.svg` is the editable artwork. `companion-body.png` is its transparent
340 × 290 browser render. `companion-body.inc` stores native BGRA pixels, embedded
in the application by `mosaico_companion_asset.cc`. This deliberately avoids PNG
decoding on each animation frame with LVGL's disabled image cache. Eyes, mouth,
breathing and tilt are live native LVGL objects; the artwork contains only the body.

To encode the checked-in PNG:

```sh
python3 scripts/mosaico-ui-sim/generate_assets.py
```

The optional `--font /path/to/NotoSansSC.otf` regenerates `mosaico_ui_small.c` using
lv_font_conv 1.5.3. It contains fixed UI strings at 20 px, with the current loaded
30 px text font as fallback. The generator requires Pillow; font conversion uses
Node/npx. Normal builds use checked-in sources and need none of these tools.
The font is derived from Noto Sans CJK SC under the SIL Open Font License;
see `NOTO-LICENSE.txt`. It must not be advertised as an original Noto font.

Both image and auxiliary font are application-embedded. The external XiaoZhi
font/emotion partition and retained partition layout are unchanged.
