<p align="right"><a href="ninehalf.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# NineHalf

ESP32-C3 / 240 × 320 application using the existing Passport BSP. Design preview: `design-preview/index.html`; local preview: `python3 -m http.server 8765 --directory design-preview`.

## Controls

- Home: UP/DOWN browses current and previous records; DOWN after the final history row returns to the top. OK refreshes. Hold UP or press UP at the top for observation; hold DOWN for settings. Holding OK does not refresh.
- Observation: focus starts on the special/plain zodiac selector. UP/DOWN reaches that selector, the sort selector and zodiac rows; OK changes the selected option. Hold UP, or press UP on the top selector, to open the zodiac reference. Hold OK returns home. The default order is consecutive misses descending; the alternative is seven-day issue count ascending.
- Zodiac reference: UP/DOWN selects one of 12 rows and wraps at either end. Short OK has no action; hold OK returns to the same observation focus. The offline 2026 table shows zodiac, earthly branch, polarity, category, numbered color tiles and the five-element label below each number. It reproduces the user-supplied reference image; it does not automatically change year or alter the record-based observation statistics. Preview: `design-preview/zodiac.html`.
- Settings: no item is selected on entry. Choose Wi-Fi management, provisioning, reminders, volume, brightness or screen sleep settings with UP/DOWN and confirm with OK. UP/DOWN adjusts a selected level; OK saves it. Hold OK returns one level.

## Network and data

Select provisioning in Settings. In WeChat open the FoloToy AI Passport Bluetooth provisioning mini program, select `BLUFI_FoloPassport`, and enter a 2.4 GHz Wi-Fi SSID/password. BLE stops after connection or 180 seconds. Up to five successfully connected networks are saved in a separate NVS namespace. The top bar shows the current SSID and signal bars.

HTTPS GET verifies the server certificate and supports identity/gzip responses. From 21:30 (inclusive) to 21:40 (exclusive) Beijing time, live requests run serially without an intentional delay after successful responses, reusing HTTPS connections where possible. Failures retry after 1/2/5 seconds. History sync pauses during this window and resumes once afterward. Outside the window, startup, Wi-Fi recovery and manual OK trigger updates; there is no periodic polling. SNTP supplies the clock; failed requests retain cached data.

Cache holds the latest 30 unique issues, ordered by response timestamps, regardless of the history URL year. The first six positions are the plain group; the seventh is the special group. Zodiac values are taken from each individual record; traditional/simplified equivalents are normalized. A plain-group zodiac is counted once per issue. The seven-day measure includes today and the preceding six Beijing calendar days. Historical counts describe the loaded sample. No future result is recommended.

Settings and cached records use a versioned NVS blob. Initial synchronization is silent; a subsequently received new issue can trigger one short audible reminder. The remembered issue is saved before playback. Brightness defaults to 80%, volume to 60%.

Settings and offline records use the `ninehalf` NVS namespace. On first boot after an upgrade, valid data from the previous firmware namespace is copied to the current namespace. Wi-Fi credentials remain in their separate namespace.

## Implementation and verification limits

The renderer creates only the current viewport's history/statistics rows and up to five zodiac-reference rows. The reference data is constant Flash data and adds no background task, network request or NVS write. The LVGL pool is 64 KiB; all 30 records remain browsable. Fonts are an OFL Noto Sans SC subset; regenerate with `tools/generate_ninehalf_fonts.py` after adding text.

Run `./tools/validate.sh` with ESP-IDF 5.5.3 activated. The verified merged image is `build/ai-passport-ninehalf-full.bin`, flashed at `0x0`. The content-addressed debug archive preserves its exact ELF and manifest. Flashing may reset saved settings. Do not flash without user confirmation.

Host checks and compilation do not establish physical display fidelity, actual RAM headroom during simultaneous Wi-Fi/BLE/TLS, API availability from the device, provisioning, sound, or long-running stability. These require device acceptance.

### Automatic screen sleep

Settings include an automatic screen sleep switch and 30 seconds / 1 / 2 / 5 / 10 minutes. Defaults: enabled, 1 minute. UP/DOWN selects, OK cycles the value; the time row is disabled when sleep is off. Preferences use separate NVS keys without changing the record cache layout. Sleep turns off the backlight, puts the LCD to sleep and pauses LVGL timers/rendering; background networking and record caching continue. The first function-button press restores the latest page before lighting the backlight, without activating its action. LCD wake requires about 100 ms. Deep sleep, automatic light sleep and power-off are not used, preserving ADC-button sampling.

CPU frequency may fall to 80 MHz while idle; the screen, HTTPS processing and reminder playback hold a 160 MHz performance lock. The audio codec/I2S are suspended at startup and after reminders. Wi-Fi uses minimum modem power saving while awake or during the 21:30–21:40 Beijing realtime window, and maximum modem power saving while the screen is asleep outside that window. Continuous realtime requests remain enabled. Offline scans start at 10-second intervals, back off to 30 seconds after three unsuccessful scans and 60 seconds after six; waking or manually refreshing resets the backoff. Reconnection can therefore take up to one scan interval after a long offline period. Actual energy savings and wake/network compatibility require device measurements.
