<p align="right"><a href="ninehalf.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# NineHalf

ESP32-C3 / 240 × 320 application using the existing Passport BSP. Design preview: `design-preview/index.html`; local preview: `python3 -m http.server 8765 --directory design-preview`.

## Controls

- Home: UP/DOWN browses current and previous records; DOWN after the final history row returns to the top. OK refreshes. Hold UP or press UP at the top for observation; hold DOWN for settings. Holding OK does not refresh.
- Observation: focus starts on the special/plain zodiac selector. UP/DOWN reaches that selector, the sort selector and zodiac rows; OK changes the selected option. Hold OK returns home. The default order is consecutive misses descending; the alternative is seven-day issue count ascending.
- Settings: no item is selected on entry. Choose Wi-Fi management, provisioning, reminders, volume or brightness with UP/DOWN and confirm with OK. UP/DOWN adjusts a selected level; OK saves it. Hold OK returns one level.

## Network and data

Select provisioning in Settings. In WeChat open the FoloToy AI Passport Bluetooth provisioning mini program, select `BLUFI_FoloPassport`, and enter a 2.4 GHz Wi-Fi SSID/password. BLE stops after connection or 180 seconds. Up to five successfully connected networks are saved in a separate NVS namespace. The top bar shows the current SSID and signal bars.

HTTPS GET verifies the server certificate. Only gzip/identity are advertised. Latest results are fetched every five minutes, every 15 seconds from 21:30 through 21:44 Beijing time, with backoff after errors. SNTP supplies the clock. Failed responses retain cached records. No authorization or cookies are used.

Cache holds the latest 30 unique issues, ordered by response timestamps, regardless of the history URL year. The first six positions are the plain group; the seventh is the special group. Zodiac values are taken from each individual record; traditional/simplified equivalents are normalized. A plain-group zodiac is counted once per issue. The seven-day measure includes today and the preceding six Beijing calendar days. Historical counts describe the loaded sample. No future result is recommended.

Settings and cached records use a versioned NVS blob. Initial synchronization is silent; a subsequently received new issue can trigger one short audible reminder. The remembered issue is saved before playback. Brightness defaults to 80%, volume to 60%.

Settings and offline records use the `ninehalf` NVS namespace. On first boot after an upgrade, valid data from the previous firmware namespace is copied to the current namespace. Wi-Fi credentials remain in their separate namespace.

## Implementation and verification limits

The renderer creates only the current viewport's history/statistics rows. The LVGL pool is 64 KiB; all 30 records remain browsable. Fonts are an OFL Noto Sans SC subset; regenerate with `tools/generate_ninehalf_fonts.py` after adding text.

Run `./tools/validate.sh` with ESP-IDF 5.5.3 activated. The verified merged image is `build/ai-passport-ninehalf-full.bin`, flashed at `0x0`. The content-addressed debug archive preserves its exact ELF and manifest. Flashing may reset saved settings. Do not flash without user confirmation.

Host checks and compilation do not establish physical display fidelity, actual RAM headroom during simultaneous Wi-Fi/BLE/TLS, API availability from the device, provisioning, sound, or long-running stability. These require device acceptance.
