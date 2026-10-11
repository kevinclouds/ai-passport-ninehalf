<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# ai-passport-ninehalf

NineHalf 是运行在 FoloToy AI Passport（ESP32-C3）上的数字游戏看板，展示本期、最近 30 期记录与观察页。支持 Wi-Fi 配网、多网络保存、离线记录、提醒，以及音量和亮度调节。

操作和数据说明见[应用文档](docs/apps/ninehalf.zh_CN.md)，其他资料见[文档索引](docs/README.zh_CN.md)。本地设计预览位于 `design-preview/`。

使用 ESP-IDF 5.5.3 构建：

```sh
./tools/validate.sh
```

合并固件为 `build/ai-passport-ninehalf-full.bin`，可从 `0x0` 刷写。刷机前请核对目标设备及[固件布局与数据策略](docs/development/engineering/firmware-layout.zh_CN.md)。
