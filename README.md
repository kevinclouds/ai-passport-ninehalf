<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# ai-passport-ninehalf

NineHalf is a number-game display for the FoloToy AI Passport (ESP32-C3). It shows the current result, the previous 30 issues, and an observation page. It supports Wi-Fi provisioning, saved networks, offline records, reminders, volume, and brightness controls.

See [application controls and data handling](docs/apps/ninehalf.md) and the [documentation index](docs/README.md). The local design preview is in `design-preview/`.

Build with ESP-IDF 5.5.3:

```sh
./tools/validate.sh
```

The verified merged firmware is `build/ai-passport-ninehalf-full.bin` for flashing at `0x0`. Flash only after checking the target device and stored-data policy in [firmware layout](docs/development/engineering/firmware-layout.md).
