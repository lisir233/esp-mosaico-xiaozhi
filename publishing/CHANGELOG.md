# 2.5.3 — ESP-Mosaico development/debug build

- 将 LVGL 局部刷新限制到单次 DMA 传输，修复下滑后保留的横纹。
- 未保存 Wi-Fi 时使用 Vibe Mode 共享配置；下拉菜单新增网络配网入口。
- 摄像头按需启动采集，关闭取景或对话暂停时停止连续采集。
- 音频重采样增加错误处理、边界检查和调试日志；ES8311 读写与启停串行化。
- Added bilingual Mosaico Ideas presentation resources and packaging metadata.

Source baseline: `12278ed` (audio diagnostics). Firmware ELF SHA-256:
`5a17432b971c24103a5d4f7628ff4b3a828ba001633ded4c71a1cd5487875a63`.

Application binary SHA-256:
`9ebccf70e9eb991fceebded041ecb007afdfda4f06f56d1eb7579599736b8d00`.

The package retains version 2.5.3 from the tested image. Camera intermittency and
warm-restart DMA behavior remain open; this is not a long-term stability release.
