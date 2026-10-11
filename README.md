<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# NineHalf

NineHalf is a number-game dashboard for the FoloToy AI Passport. It brings the current issue, recent records, and zodiac observations onto the device’s 240 × 320 display.

| Current issue and records | Last 30 issues |
| --- | --- |
| ![Home screen](design-preview/native/home.png) | ![Observation screen](design-preview/native/observe.png) |

*Native-rendered previews; see [the interactive design preview](design-preview/index.html) for more screens.*

## What it does

- Shows the current issue, a countdown, and a 30-issue record window.
- Separates the first six numbers (plain group) from the seventh (special group).
- Summarizes zodiac appearances, recent activity, and consecutive absences across the loaded 30 issues. The observations describe past records; they do not recommend future results.
- Saves records locally so they remain available offline. Up to five Wi-Fi networks can be saved for automatic reconnection.
- Provides a new-issue reminder and on-device volume and brightness controls.

## Controls and Wi-Fi

On Home, use UP/DOWN to browse records and press OK to refresh. Hold UP—or press UP while already at the top—to open Observation. Hold DOWN to open Settings. On other pages, hold OK to return.

To connect, open **Settings → Wi-Fi provisioning**, then use the FoloToy AI Passport Bluetooth provisioning WeChat mini program to connect the device to a **2.4 GHz Wi-Fi** network. Saved networks can be selected or deleted in **Settings → Wi-Fi Management**.

For detailed controls and data handling, see the [application guide](docs/apps/ninehalf.md).

## Build and preview

Target hardware: **ESP32-C3, 8 MB Flash, no PSRAM**. Activate **ESP-IDF 5.5.3**, then run:

```sh
./tools/validate.sh
```

The validated merged image is `build/ai-passport-ninehalf-full.bin`, intended for flashing at `0x0`. Check the [firmware layout and stored-data policy](docs/development/engineering/firmware-layout.md) before flashing.

To view the interactive design preview locally:

```sh
python3 -m http.server 8765 --directory design-preview
```

Open `http://127.0.0.1:8765/`. More project and development documents are listed in the [documentation index](docs/README.md).
