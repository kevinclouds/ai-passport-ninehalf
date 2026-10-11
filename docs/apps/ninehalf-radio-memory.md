<p align="right"><a href="ninehalf-radio-memory.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Radio memory correction — 2026-10-05

The flashed diagnostic image captured UI dispatch and a ready network worker, followed by HCI initialization failure inside NimBLE. The internal heap before controller initialization was about 42 KiB. HCI initialization allocates MSYS/transport pools, registers the VHCI callback and creates a semaphore; the remaining RAM budget makes allocation failure the leading hypothesis, not yet a confirmed allocator trace.

Changes: retain the 64 KiB LVGL pool; reduce Wi-Fi static RX buffers from 10 to 6 (still at least the RX BA window), cap dynamic RX/TX at 8, reduce single-peer BLE MSYS2 and ACL pools from 24 to 8 and high-priority event buffers from 30 to 10. This favors memory availability over peak throughput. Allocate the parsed-response snapshot only during HTTP requests, so provisioning does not retain it. Add a ROM-print allocation-failure hook recording function/size/capabilities without allocating or printing credentials. Remove the duplicate LVGL pool default.

Build: PASS. Full gate and independent archive verification; merged image 2,093,456 bytes at `0x0`. Host tests: PASS. Effective isolated-build Kconfig values were checked against the intended limits. Device tests: FAIL. After authorized flashing and Flash hash verification, the matching boot ELF prefix `77ec0d491` was captured. Provisioning reached NimBLE/GATT initialization with 53,968 bytes free before startup, but allocation of the 4,608-byte BTC task stack failed (`caps=0x804`), triggering the ESP-IDF `esp_blufi_btc_init` assertion and reboot. This confirms an allocation failure. Scanning, BLUFI security negotiation, Wi-Fi connection, device HTTPS/gzip and runtime stability remain unverified.

Working tree: `main`, uncommitted. The 180-second diagnostic monitor has closed; this image was flashed and failed provisioning.

Full SHA-256: `b7854e2a3c5e64d05c1cc7ff8398d19729f4b0680e575e68d0cb0ec4c82290d1`

ELF SHA-256: `77ec0d4910639a6879c39943318a8c4caf99e813c43c3cf19fd282cdbe6fe07f`

Archive: `build/firmware/b7854e2a3c5e64d05c1cc7ff8398d19729f4b0680e575e68d0cb0ec4c82290d1`

## BTC task follow-up candidate

The next candidate defers `bsp_audio_init()` until the first reminder, keeping the BSP and volume persistence unchanged. The unneeded startup I2S DMA allocations no longer compete with provisioning. Before the asserting `esp_blufi_btc_init()` call, log the suitable heap and reject startup with `ESP_ERR_NO_MEM` if the total is below 16 KiB or the largest block below 4,608 bytes. This reserves budget for the 4,608-byte BTC stack, 4,096-byte host stack and protocol allocations; it is not a guarantee for subsequent pairing allocations. Existing central/observer roles were already disabled.

A 56 KiB LVGL experiment failed during native widget creation and was reverted. Keep the verified 64 KiB pool; native real-data page/navigation/selection/settings stress passes. Build: PASS; Host tests: PASS; Device tests: NOT RUN for this candidate. Audio on first reminder and subsequent reprovisioning after audio initialization also require physical testing.

Merged image: 2,093,648 bytes at `0x0`. Full SHA-256: `33065ac15171bf7ea688da8803b42d7df277dd6069b483dbae42bde6e6290953`. ELF SHA-256: `da0004a88918792bbf84dcce3c132b4c692ba4cee4ad82d06ccc3c980bf27784`. The independent archive verification and archived map confirm image identity and 64 KiB LVGL pool. This candidate was flashed with authorization and verified. The matching ELF prefix `da0004a88` was logged. Before NimBLE startup, heap was 67,716 bytes; after GATT, 18,996 bytes remained, largest block 10,752 bytes. BTC and host tasks initialized without the former assertion. Device tests: FAIL for provisioning acceptance: a 30-byte controller DMA allocation (`caps=0x808`) failed while setting advertisement data, returning `BLE_ERR_MEM_CAPACITY`; the user observed “waiting for phone” but the mini program found no device. The status did not prove successful advertising.

## Controller activity budget candidate

The controller still used the ESP-IDF default of six activity instances. Its local Kconfig documents 828 bytes per instance. This candidate sets `CONFIG_BT_CTRL_BLE_MAX_ACT=2` for advertising and a single connection, reducing the documented instance budget by 3,312 bytes. It retains the 64 KiB LVGL pool and delayed audio initialization. Check `ble_gap_adv_active()` after starting advertising; report failure to the UI instead of treating profile initialization as successful broadcasting, with suitable DMA heap diagnostics.

Build: PASS; Host tests: PASS; independent archive verification: PASS. Device tests: NOT RUN for this candidate; the prior image failed broadcasting despite successful host initialization. Unverified: whether the controller budget suffices for actual advertising, discovery, BLUFI negotiation, Wi-Fi connection, HTTPS/gzip and reminder/reprovisioning stability. The previous monitor has closed. This candidate was flashed with authorization at `0x0`; esptool verified the Flash hash, and the boot ELF prefix `20153a2f6` matches the archive. The new 180-second monitor is collecting logs. At the current check, no provisioning request has been received; advertising/discovery acceptance is pending user operation.

Merged image: 2,093,872 bytes at `0x0`; full SHA-256 `2ff987042d7844a5259a6202806df5032299d8d1dd368f47cc075593cf5984f8`; ELF SHA-256 `20153a2f67946d58e9e3be732f5a32bf2c6211a2d3eaf4f31e359f2c4c6243cb`. Changes remain uncommitted on `main`.

Device follow-up for `2ff98704`: the matching firmware reached active advertising and established the BLE connection (user-confirmed). Post-GATT internal heap was 22,472 bytes, largest block 14,336 bytes. During encrypted BLUFI response, `heap_caps_aligned_alloc` failed for 174 bytes with `caps=0x8`, followed by `esp-aes: Failed to allocate memory` and `btc_blufi_send_encap encrypt error -1`. The user reported the mini program’s “device cannot encrypt provisioning data” message and an empty Wi-Fi list. This confirms AES DMA allocation failure; it does not establish that Wi-Fi scanning itself returned zero networks.

## AES DMA candidate

Reduce the BSP single LCD draw buffer from 40 to 20 lines (19.2 to 9.6 KiB); preserve BSP drawing, rounding, orientation, LVGL 64 KiB pool and page layouts. Trade-off: more flush batches for 9.6 KiB DMA headroom. Move the 16-entry AP records/list off the event task stack into checked temporary allocations; release records before queueing the list and release the list after the BLUFI API copies it. Log scan request status, result count and queue status without SSIDs or passwords.

Build: PASS; Host tests: PASS; archive verification: PASS. Device tests: NOT RUN for this candidate. Unverified: LCD refresh timing, AES negotiation/list response, actual Wi-Fi connection, device HTTPS/gzip, reminder and subsequent reprovisioning. Full merged image 2,094,624 bytes at `0x0`, SHA-256 `6130407fc81ec66d1b8b506ba65588585699a268ea64102850aa598b3d193317`; matching ELF `db9c428d470473ab857e74e4f181150a6c4452a5e16a2ab9dbacc9d104a54d42`. Flashed with explicit authorization at `0x0`, Flash hash verified, and boot ELF prefix `db9c428d4` matches the archive. The new 180-second monitor is ready; no provisioning request has been captured at this check. Actual AES/list/connection acceptance is pending user operation. Changes remain uncommitted on `main`.

Device follow-up for `6130407f`: BLE advertising, connection, encrypted AP list transmission and Wi-Fi IP acquisition have passed. The AP scan returned 11 records and the list queued with `ESP_OK`; suitable DMA heap then was 10,324 bytes, largest block 7,168 bytes. The user confirmed Wi-Fi connected. Data synchronization remains FAIL: after automatic Wi-Fi connection on boot, the matching ELF logged repeated 32,768-byte allocation failures followed by `sync failed`. The source gzip output window is this size; the allocator hook alone does not include the call site. The device also logged one NimBLE shutdown connection-termination error during provisioning teardown; subsequent boot connection worked, but repeated teardown remains unverified.

## Identity response candidate

Both public HTTPS endpoints were independently requested with `Accept-Encoding: identity`: live HTTP 200, 447 bytes; history HTTP 200, 163,257 bytes; neither had Content-Encoding. The firmware now requests identity while retaining gzip fallback, avoiding the 32 KiB gzip window on compliant identity responses. Certificate verification remains enabled. Add HTTP/open/header/body/parser diagnostics without credentials.

Build: PASS; Host tests: PASS; archive verification: PASS. Device tests: NOT RUN for this candidate. Unverified: device HTTP identity response, live/history parsing and display, reminders, repeated provisioning teardown; gzip fallback still needs device memory validation if the server forces gzip. Monitor closed. Merged image 2,095,152 bytes at `0x0`, full SHA-256 `cbe1bbce23b756f6fa5bd8385abfbe020cff153c00f6eb8794b9e076de9a76d6`, ELF `9011e1fc14d723ef9f6a006a87346dccf4b137996a9f4b70d812f619ec7d77b6`. Flashed with explicit authorization at `0x0`; Flash hash verified and boot ELF prefix `9011e1fc1` matches. Initial logs have not yet shown Wi-Fi IP acquisition; user was asked to reprovision after the merged flash. Actual device response/display acceptance remains pending. Changes remain uncommitted on `main`.
