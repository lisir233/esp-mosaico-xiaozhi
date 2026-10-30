# ESP-Mosaico

<div align="center">
    <a href="https://mosaico.espressif.com">
        <b> ESP-Mosaico 网站 (文档等) </b>
    </a>
    |
    <a href="./README.md">
        <b> English </b>
    </a>
</div>

## 简介

ESP-Mosaico 是乐鑫基于 ESP32-S31 打造的可扩展智能交互开发套件，面向磁吸扩展、运动感知、方形触摸屏与端侧多媒体等应用场景。设备搭载 480 × 480 QSPI 方形触摸屏、音频编解码与功放、六轴 IMU、双磁力计、SPI NAND flash，并提供左右两个 2 × 10P、2.54 mm 标准间距端子的模块接口，可用于摄像头等功能扩展。

ESP-Mosaico 有不同的硬件版本，当前版本的小智固件已支持 ESP-Mosaico "Early Makers" (`v1.0`) 和 ESP-Mosaico 正式版 (`v1.2`)，支持 OV3640 和 SC101IOT 两种摄像头子版。

> [!NOTE]
>
> 当前版本的小智固件暂不支持子版热插拔，**如需使用摄像头功能，请在启动前插入摄像头子版**。

## 编译

目前 ESP-IDF 暂未发布 v6.2 正式版，在 ESP-IDF v6.2 正式发布前，推荐使用 `master` 分支 `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` commit 的 ESP-IDF 编译。

```sh
source /path/to/esp-idf-master/export.sh
python3 scripts/build.py espressif/esp-mosaico
```

当本仓库位于 ESP-Mosaico 工作区的 `projects/` 目录时，根 CMake 配置还会
保留 Vibe Mode，在正常应用中启用 ESP-Iris，并将生成的小智资源打入产品
System Update 包。请在工作区根目录执行：

```sh
python3 mosaico.py iris system-update --project projects/xiaozhi-esp32
```

UI 必须同时加载小智的 `sdkconfig.defaults`、ESP32-S31 配置和 Mosaico
产品配置。通用配置中的 `LV_FONT_FMT_TXT_LARGE` 与 CBIN 字体资源的结构一致，
`LV_USE_LODEPNG` 用于表情 PNG；缺失时会出现图标正常、文字和表情空白。
根 CMake 会检查这两项，旧构建需要通过新的 `SDKCONFIG` 路径重新生成配置，
仅追加 defaults 不会覆盖已有配置。`cmake/mosaico-retained.defaults` 保留
bootloader 的体积优化，以符合现有固定分区边界，同时保留工作区原有的应用
debug 优化配置：验证中切换到通用的体积优化后，固定版本的 ESP32-S31 构建
曾在语音模型初始化时崩溃。

ESP-Iris 截图在独立任务中完成 LVGL 渲染和 JPEG 编码，避免占满协议任务栈。
截图使用原生 RGB565 字节序，适用于 CLI 截图和小智截图工具。

## 陪伴界面与交互

480 × 480 原生 LVGL 界面使用柔白、品牌橙与原创陪伴角色。轻触角色或底部
聊天按钮开始对话；说话时角色缩小，为正文让出空间。长回复可滑动阅读，右下
角打开最近 20 条消息（每条约 2 KB，UTF-8 边界截断）；结束对话不会清空历史。
历史仅保存在 RAM，重启清空。顶部下拉或点击标题打开音量、亮度和清空记录。

左下角打开左侧 CameraBoard 取景，橙色快门拍照识图，结果回到对话；“提问”
返回语音输入。识图服务地址通过正常的小智 MCP 初始化获得，因此首次拍照前
需成功连接一次语音服务。缺少相机、拍照失败和服务错误都会在页面提示。为优先
保障语音，持续取景仅在空闲时运行，对话中暂停但仍可单次拍照；取景不是录像。

取景目标为每秒 10 帧，输出与窗口一致的 432 × 288 RGB565。PPA 使用 BSP 的
左侧安装旋转/镜像方向，将传感器中央 384 × 576 区域等比缩小至 0.75 倍，避免
拉伸。每次借用一帧传感器缓冲，复用两块带引用计数的输出缓冲；LVGL 在锁内
替换图像前持有缓冲，PPA 不会写入仍被显示使用的图像。识图仍使用原始
1280 × 720 图像。每五秒记录帧率及采集/转换/呈现耗时，实际帧率取决于传感器、
显示和语音负载。

BMI270 提供轻微视线跟随与拿起反馈。拿起只唤亮并轻振，不会自动打开麦克风。
马达脉冲限定 10–80 ms，由独立 esp_timer 关闭；传感器读取失败不会延长振动。

模块由板级对象持有，生命周期持续到设备重启：`MosaicoUi` 只负责视图和有界
历史，所有调用持有 LVGL 锁；`MosaicoDisplay` 适配字体、截图和帧所有权，并在
渲染前按 BSP 的横向 4 像素规则对齐局部刷新区域，保证绘制缓冲与 CO5300 写入
窗口一致。模拟器检查设置页、滚动和相机更新的实际刷新区域均正确对齐；
`MosaicoCamera` 用独立低优先级任务取景，互斥锁将采集与识图作为一次事务，
避免 MCP 与预览覆盖彼此的图像；`MosaicoFeedback` 在独立任务采样共享 I2C
总线。UI 回调仅投递主循环，不在 LVGL 回调中执行 HTTP、音频生命周期或 I2C。

LCD 总线沿用 BSP 修复 `f629a8e` 与 `e1db301`：每个 DMA 分块申请 8 行
RGB565（7680 字节），关闭 PSRAM 直接 DMA，并在 40 MHz 下显式将 SCL/
DATA0–3 的驱动强度设为 `GPIO_DRIVE_CAP_DEFAULT`。传输设置与区域对齐均需
保留；LVGL 软件截图正常不代表物理屏幕传输正常，残留横条仍须实屏验收。

可通过 `scripts/mosaico-ui-sim` 运行同一份真实界面代码。依赖系统 SDL2、CMake、
C++20 与 Noto CJK 字体；LVGL 使用本项目已解析的 managed component：

```sh
cmake -S scripts/mosaico-ui-sim -B .codex-runs/ui-sim -G Ninja
cmake --build .codex-runs/ui-sim
cd .codex-runs/ui-sim
./mosaico_ui_sim               # 触摸/滑动断言及 PPM 截图
./mosaico_ui_sim --interactive # 可交互窗口
```

角色图像和辅助字体嵌入应用，见 `assets/README.md`。修改这些资源无需更新外部
资源分区；设备更新方式仍须由工作区 CLI 检查完整分区表和产品兼容性。
