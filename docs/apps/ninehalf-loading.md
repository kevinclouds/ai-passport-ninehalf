<p align="right"><a href="ninehalf-loading.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Content-area loading

The user approved loading across the content area. On home, replace the record region with a centered animated spinner and Chinese status, retaining the status bar and footer. Display clock synchronization while time is invalid. Manual OK immediately starts loading when online, repeated OK while busy does not enqueue another refresh. The worker posts start/finish messages through the existing UI queue, holding the loading view until the live/history request sequence completes. Failed requests restore cached records and the existing failure header. Long OK still opens the menu. No records are discarded by loading.

The preceding `cbe1bbce` firmware passed device identity HTTPS: live 200/1 parsed record, history 200/30 records, `sync ok`, about 21 seconds after startup. The loading image was flashed with explicit authorization at `0x0`; esptool verified the Flash hash and the boot ELF prefix `6a25db704` matches. A 180-second monitor is active. Initial logs have not yet shown Wi-Fi IP acquisition; on-device visual acceptance awaits user operation.

Build: PASS. Host tests: PASS, including native loading/failure/restoration and existing navigation/settings stress; loading consumes 12,584 bytes of the retained 64 KiB LVGL pool. Chinese fonts regenerated and coverage verified (233 glyphs/12 sizes). Device tests: NOT RUN for loading. Unverified: on-device spinner animation, slow/error transitions, reminders and repeated provisioning teardown.

Merged image: 2,101,616 bytes at `0x0`. Full SHA-256 `d17ce335b1a65439a93b042f3c55382b389a94d98398ac5a651001de2a167197`. ELF SHA-256 `6a25db70498b6720f198683ddd9e3cf6ff7caa6dd7676decde7134dee7a3770f`. Native preview: `design-preview/native/loading.png`, gallery `http://127.0.0.1:8765/native.html`. Changes remain uncommitted on `main`.

## Continuous rotation correction

Replace the default variable-length spinner with a fixed 90-degree arc, linear 1,000 ms rotation, infinite repeat and zero repeat delay. Retain the loading area and animation on intermediate renders; update only the status text. Native tests advance 120 x 20 ms across cycles and verify angular steps and root/arc identity after intermediate renders. Full gate, host/native tests and independent archive verification pass. Device smoothness remains NOT RUN. Full SHA-256 `b0068579a32a33343f8bb56b93af766e4c59e3049cdeea194fadb48981fcd7a4`, ELF `dbd8de8680ea1ac75a7665c384426216f99d76d4dc92799446724a3b05669394`, merged 2,101,920 bytes at `0x0`; not flashed, uncommitted.

The continuous-rotation image was flashed with explicit authorization at `0x0`. Esptool verified the Flash hash; the logged ELF prefix `dbd8de868` matches its archive. A 180-second monitor is active. Visual continuity and post-refresh restoration are pending user confirmation; do not treat host animation checks as device acceptance.

Return-home fix: reproduce a background synchronization started on Observation, then long OK home. No new refresh request was sent, but the shared syncing flag displayed full-area loading. Separate foreground loading from background synchronization. With cached records, background sync preserves browsing; manual OK or initial empty data retains full loading. Leaving Home clears foreground loading. Native regression verifies return without refresh/arc, history navigation during background sync, manual loading on the existing request, and the retained spinner lifecycle. Build/Host/native/archive: PASS. Candidate adc0af033b9c924658f7dab8b73fae1ff468b64db553219d10ec0675a46491f6 not flashed; physical return-home acceptance NOT RUN; uncommitted.

adc0af03 flashed with explicit authorization using verified compatible segments preserving NVS. Flash hashes verified; boot ELF prefix 525479b75 matches. Automatic Wi-Fi connection and live HTTPS 200/one parsed record passed. History HTTPS 200 observed without parse/sync completion in the bounded 55-second window. Serial monitor closed. Physical return/manual-refresh acceptance pending user feedback. Uncommitted.

User confirmed adc0af03 returns home directly to cached records and short OK still shows manual-refresh loading; both physical checks passed.
