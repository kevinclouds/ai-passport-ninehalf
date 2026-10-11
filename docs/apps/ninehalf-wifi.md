<p align="right"><a href="ninehalf-wifi.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Saved Wi-Fi and offline records

The device remembers up to five successfully connected networks in the `nt-wifi` NVS namespace. A repeated SSID updates its password and moves to the most recently used position. A sixth network replaces the least recently used network. Existing SDK credentials migrate before the legacy SDK slot is cleared; subsequent STA configurations use RAM so a deleted legacy network cannot return after reboot. Application record/settings storage stays separate.

On startup or loss of connection, the network worker scans visible networks and chooses the strongest saved SSID. Failed candidates are skipped until the list is exhausted, with a 20-second connection timeout and a 30-second pause when no candidate remains. A healthy connection stays connected. A successful manual selection stays connected until disconnection, then automatic selection resumes. Automatic scans stop during BLUFI provisioning; no Wi-Fi work blocks the LVGL button callback.

## Controls

From Home, hold DOWN for Settings. Settings starts without selection. Select the Wi-Fi row and press OK to enter management. The header shows the current SSID and four signal bars. Saved rows show connected status, last scan RSSI, or unavailable scan status. UP/DOWN selects a row; OK opens Connect/Delete. Delete opens Cancel/Confirm Delete with Cancel selected. Deleting the connected network disconnects it and allows another saved network to connect. Long OK returns one level. Commands queued behind synchronization show a processing status.

Long SSIDs use ellipsis. Network-name labels use a dedicated 13-pixel font covering the source font's BMP characters, including Chinese, with LVGL large-font format. Other interface fonts retain their existing subsets.

## Offline behavior

The existing last-30-issue NVS record cache loads after reboot. Home, history, and observation remain available offline. Manual refresh reports unavailable connectivity without clearing records. Reconnection schedules an immediate live/history synchronization. An offline reboot has no verified wall-clock time until network time synchronization succeeds; the countdown reports the unsynchronized state.

## Validation scope

Host tests cover same-SSID updates, capacity eviction, deletion, signal ordering and failed-candidate exclusion. Native UI tests cover management/actions, deletion cancellation and confirmation, explicit connect, no default Settings action, existing shortcuts, offline refresh/cache retention, and Chinese name glyph coverage. Physical multi-router roaming, credential migration/reboot persistence and deletion of the currently connected network require device acceptance after an authorized flash.

Build, Host tests, native viewport/interaction tests, JavaScript syntax and archive verification: PASS. Verified merged image SHA-256 c04ab5b6fe7f92d4c28da83b52e688b6e34dfe4a6aaee5413ee9b12547a63ba4, application 5,102,720 bytes, merged 5,168,256 bytes at 0x0. Partition table is byte-identical to installed b7c55088, permitting segmented updates preserving NVS. Not flashed; device acceptance NOT RUN; changes uncommitted.

c04ab5b6 flashed with explicit authorization using verified compatible segments, preserving NVS. Flash hashes verified; boot ELF prefix aee29b996 matches. Automatic connection without re-provisioning and live HTTPS 200/one parsed record passed. History HTTPS 200 observed, but parse/sync completion was not captured within 55 seconds. No crash/save failure observed. Serial monitor closed. Management visuals, second-network provisioning/manual switching, deletion, offline/reboot and roaming remain pending physical acceptance. Uncommitted.

Provisioning-priority fix: on-device logs showed UI provisioning requests at 17.7/18.3 seconds, while the worker was still downloading history and did not dispatch them in the observation window. Set a pending flag immediately, show Starting in Settings, and check it between HTTP stages/body reads. Body reads use a 1-second timeout with EAGAIN retries within a 120-second total budget, releasing HTTP/stream/snapshot resources before BLUFI startup. TLS/header phases retain their existing 15-second timeout. Intentional cancellation does not report a sync failure. Native pending-state regression, Build/Host/archive: PASS. Candidate 5da0024a37b2f16f2dface9c46f3dab5893d9ea36ce4f1ed6e82c18733ef020f not flashed; physical dispatch latency/BLE acceptance NOT RUN; uncommitted.

5da0024a flashed with explicit authorization using compatible verified segments; NVS preserved and boot ELF prefix 175b6f0ee matched. User confirmed Waiting for phone, found BLE but connection failed. Recovered serial observation showed request 8893 ms, dispatch 9367 ms and advertising active 9398 ms, then advertising stopped at 11400 ms. Existing automatic Wi-Fi GOT_IP incorrectly completed provisioning even without a phone AP-connect request. Gate completion on an explicit BLUFI AP-connect request and an actual AP SSID matching the pending configuration; only matching successful connections save credentials. Build/Host/archive: PASS. Candidate 9c2e9e60e7e73bae0d4943bc4f221e0d25229b768de3088b98a3a7bdd766fd19 not flashed; sustained advertising/phone connection acceptance pending. Serial monitors closed; uncommitted.

9c2e9e60 flashed with explicit authorization using verified compatible segments, preserving NVS. Flash hashes verified; boot ELF prefix 6f86d1bfd matches. Automatic Wi-Fi and live HTTPS 200/one parsed record passed. History HTTP 200 observed without completion in 55 seconds. No provisioning request captured in this window; sustained BLE and phone/AP-list acceptance pending user feedback. Serial monitor closed; uncommitted.
