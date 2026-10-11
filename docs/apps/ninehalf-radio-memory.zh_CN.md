<p align="right"><strong>简体中文</strong> · <a href="ninehalf-radio-memory.md">English</a></p>

# 无线内存调整 — 2026-10-05

实机诊断已确认：UI 请求送到正常运行的联网任务，随后 NimBLE 的 HCI 初始化失败，未启动广播。控制器启动前内部堆约 42 KiB。HCI 初始化包含 MSYS/传输缓冲分配、VHCI 回调注册和信号量创建；内存不足是主要怀疑点，尚未取得明确的分配失败日志。

改动：保留 64 KiB LVGL 池；Wi-Fi 静态 RX 缓冲从 10 降至 6，仍不低于 RX BA 窗口，动态 RX/TX 上限为 8；单连接 BLE 的 MSYS2 和 ACL 缓冲从 24 降至 8，高优先级事件缓冲从 30 降至 10。取舍是减少峰值吞吐以保留内存。响应解析缓冲改为仅在 HTTP 请求期间分配，配网时不保留；新增不分配内存的 ROM 输出分配失败钩子，记录函数、大小和 capabilities，不记录凭据；移除重复 LVGL 默认项。

Build：PASS。完整门禁及独立归档校验通过，合并镜像 2,093,456 字节，从 `0x0` 写入。Host tests：PASS，隔离构建中的实际 Kconfig 值已核对。Device tests：FAIL。授权刷入并通过 Flash hash 校验后，启动 ELF 前缀 `77ec0d491` 与归档一致。配网已到达 NimBLE/GATT 初始化，启动前剩余堆 53,968 字节，但 BTC task 的 4,608 字节栈分配失败（`caps=0x804`），触发 ESP-IDF `esp_blufi_btc_init` 断言并重启。内存分配失败已经确认。广播发现、BLUFI 安全协商、Wi-Fi 连接、设备 HTTPS/gzip 及运行稳定性仍未验证。

代码在 `main`，未提交。180 秒诊断串口监听已关闭，本镜像已刷入，配网失败。

Full SHA-256: `b7854e2a3c5e64d05c1cc7ff8398d19729f4b0680e575e68d0cb0ec4c82290d1`

ELF SHA-256: `77ec0d4910639a6879c39943318a8c4caf99e813c43c3cf19fd282cdbe6fe07f`

Archive: `build/firmware/b7854e2a3c5e64d05c1cc7ff8398d19729f4b0680e575e68d0cb0ec4c82290d1`

## BTC task 后续候选版本

后续候选版将 `bsp_audio_init()` 延后到首次提醒时，继续复用 BSP，音量保存方式不变。启动时不再提前占用 I2S DMA 内存。在会触发断言的 `esp_blufi_btc_init()` 前记录可用堆；总量不足 16 KiB 或最大块不足 4,608 字节时，以 `ESP_ERR_NO_MEM` 拒绝启动，为 4,608 字节 BTC 栈、4,096 字节 host 栈及协议分配留出预算。这不能保证后续安全协商的所有内存分配。central/observer 角色原本已关闭。

尝试 56 KiB LVGL 池时，原生页面创建测试失败，已撤销。保持 64 KiB，真实数据页面、导航、选中状态及设置压力验证通过。Build：PASS；Host tests：PASS；Device tests：本候选版 NOT RUN。首次提醒音频，以及音频初始化后的再次配网，也需要实机验证。

合并镜像 2,093,648 字节，从 `0x0` 刷入。Full SHA-256：`33065ac15171bf7ea688da8803b42d7df277dd6069b483dbae42bde6e6290953`。ELF SHA-256：`da0004a88918792bbf84dcce3c132b4c692ba4cee4ad82d06ccc3c980bf27784`。独立归档校验及归档 map 已确认镜像身份和 64 KiB LVGL 池。本候选版已授权刷入并校验，启动 ELF 前缀 `da0004a88` 与归档一致。NimBLE 启动前堆为 67,716 字节；GATT 后剩余 18,996 字节，最大块 10,752 字节。BTC 和 host task 已初始化，未复现原断言。Device tests：配网验收 FAIL。设置广播数据时，控制器 30 字节 DMA 分配失败（`caps=0x808`），返回 `BLE_ERR_MEM_CAPACITY`；用户看到「等待手机」，小程序仍搜不到。该状态不能证明广播成功。

## 控制器活动实例预算候选版

控制器仍使用 ESP-IDF 默认的 6 个活动实例，本地 Kconfig 明确每实例占用 828 字节。候选版设置 `CONFIG_BT_CTRL_BLE_MAX_ACT=2`，用于广播及单连接，按文档减少 3,312 字节预算。保留 64 KiB LVGL 池及延后的音频初始化。广播启动后检查 `ble_gap_adv_active()`；失败时向 UI 报错并记录 DMA 可用堆，不再把 profile 初始化当作广播成功。

Build：PASS；Host tests：PASS；独立归档校验：PASS。Device tests：本候选版 NOT RUN，前一版 host 初始化成功但广播失败。Unverified：控制器预算能否支持实际广播发现、BLUFI 协商、Wi-Fi 连接、HTTPS/gzip，以及提醒和再次配网的稳定性。此前监听已关闭。本候选版已授权从 `0x0` 刷入，esptool 的 Flash hash 校验通过，启动 ELF 前缀 `20153a2f6` 与归档一致。新的 180 秒监听正在采集；当前尚未收到配网请求，广播发现验收等待用户操作。

合并镜像 2,093,872 字节，从 `0x0` 刷入；full SHA-256 `2ff987042d7844a5259a6202806df5032299d8d1dd368f47cc075593cf5984f8`；ELF SHA-256 `20153a2f67946d58e9e3be732f5a32bf2c6211a2d3eaf4f31e359f2c4c6243cb`。改动仍在 `main`，未提交。

`2ff98704` 实机后续：匹配固件已启动广播并建立 BLE 连接，用户也确认连接成功。GATT 后内部堆 22,472 字节，最大块 14,336 字节。发送 BLUFI 加密响应时，`heap_caps_aligned_alloc` 的 174 字节分配失败（`caps=0x8`），随后出现 `esp-aes: Failed to allocate memory` 和 `btc_blufi_send_encap encrypt error -1`。用户反馈小程序提示「设备无法加密配网数据」，Wi-Fi 列表为空。确认的是 AES DMA 分配失败，不能据此断言扫描没有找到网络。

## AES DMA 候选版

BSP LCD 单绘制缓冲从 40 行减到 20 行（19.2 降至 9.6 KiB）；复用原 BSP 绘制、圆角、方向及 64 KiB LVGL 池，页面布局不变。取舍是增加分批刷新次数，腾出 9.6 KiB DMA 内存。16 项 AP 记录及发送列表移出事件任务栈，采用检查过的临时分配；发送前释放记录，BLUFI API 复制列表后释放列表。新增扫描请求、结果数量及排队发送状态日志，不记录 SSID 或密码。

Build：PASS；Host tests：PASS；归档校验：PASS。Device tests：本候选版 NOT RUN。Unverified：LCD 刷新时序、AES 协商及列表响应、Wi-Fi 实际连接、设备 HTTPS/gzip、提醒和再次配网。合并镜像 2,094,624 字节，从 `0x0` 刷入，SHA-256 `6130407fc81ec66d1b8b506ba65588585699a268ea64102850aa598b3d193317`；匹配 ELF `db9c428d470473ab857e74e4f181150a6c4452a5e16a2ab9dbacc9d104a54d42`。已明确授权从 `0x0` 刷入，Flash hash 校验通过，启动 ELF 前缀 `db9c428d4` 与归档一致。新的 180 秒监听已就绪；当前尚未捕获配网请求。AES、列表及连接验收等待用户操作。改动在 `main`，未提交。

`6130407f` 实机后续：BLE 广播、连接、加密 AP 列表发送及 Wi-Fi 获取 IP 已通过。扫描得到 11 条记录，列表发送排队返回 `ESP_OK`；此时 DMA 堆 10,324 字节，最大块 7,168 字节。用户确认 Wi-Fi 已连接。数据同步仍 FAIL：启动后自动连接 Wi-Fi，匹配 ELF 记录到重复的 32,768 字节分配失败，随后 `sync failed`。源码 gzip 输出窗口正好是该大小，但分配钩子本身没有调用位置。配网结束时曾记录一次 NimBLE 停止连接错误，后续启动连接正常；反复退出配网的稳定性仍需验证。

## Identity 响应候选版

分别用 `Accept-Encoding: identity` 实测两个公开 HTTPS 接口：实时 HTTP 200、447 字节，历史 HTTP 200、163,257 字节，均无 Content-Encoding。固件改为请求 identity，保留 gzip 兼容路径；服务器返回 identity 时不再需要 32 KiB gzip 窗口。保持证书校验，新增 HTTP 打开、响应头、响应体和解析诊断，不记录凭据。

Build：PASS；Host tests：PASS；归档校验：PASS。Device tests：本候选版 NOT RUN。Unverified：设备实际 identity 响应、实时及历史解析显示、提醒、反复退出配网；服务器强制 gzip 时的兼容路径仍需验证内存。监听已关闭。合并镜像 2,095,152 字节，从 `0x0` 刷入，full SHA-256 `cbe1bbce23b756f6fa5bd8385abfbe020cff153c00f6eb8794b9e076de9a76d6`，ELF `9011e1fc14d723ef9f6a006a87346dccf4b137996a9f4b70d812f619ec7d77b6`。已明确授权从 `0x0` 刷入，Flash hash 校验通过，启动 ELF 前缀 `9011e1fc1` 匹配。初始日志尚未显示 Wi-Fi 获取 IP，已请用户在合并刷写后重新配网。设备响应及显示验收待验证。在 `main`，未提交。
