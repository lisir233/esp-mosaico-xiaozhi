#!/usr/bin/env python3
"""Encode the authored companion PNG and generate the fixed UI font (dev only).

Pillow is required. Font generation additionally needs lv_font_conv 1.5.3 and
an extracted Noto Sans CJK SC OTF passed as --font. No network access at build time.
"""
from pathlib import Path
import argparse
import subprocess
from PIL import Image

BOARD = Path(__file__).resolve().parents[2] / 'main/boards/espressif/esp-mosaico'
parser = argparse.ArgumentParser()
parser.add_argument('--font', type=Path)
args = parser.parse_args()
# Native BGRA avoids decoding PNG on every animation frame when LVGL cache is off.
raw = Image.open(BOARD / 'assets/companion-body.png').convert('RGBA').tobytes('raw', 'BGRA')
(BOARD / 'assets/companion-body.inc').write_text('\n'.join(
    ','.join(f'0x{v:02x}' for v in raw[i:i+32]) + ',' for i in range(0, len(raw), 32)) + '\n')
if args.font:
    chars = set(''.join(p.read_text() for p in BOARD.glob('*.cc')) +
                '轻触我，或说「你好小智」正在连接正在聆听正在说话服务暂不可用，请重试准备好了正在回答正在陪你聊对话中，取景暂停；可直接拍照')
    symbols = ''.join(sorted(c for c in chars if ord(c) > 127))
    subprocess.run(['npx', '--yes', 'lv_font_conv@1.5.3', '--size', '20', '--bpp', '4',
                    '--format', 'lvgl', '--font', str(args.font), '--range', '0x20-0x7e',
                    '--symbols', symbols, '--no-compress', '--lv-font-name', 'mosaico_ui_small',
                    '--lv-include', 'lvgl.h', '-o', str(BOARD / 'mosaico_ui_small.c')], check=True)
