<p align="right"><strong>简体中文</strong> · <a href="ninehalf.md">English</a></p>

# NineHalf

使用原有 Passport BSP，目标为 ESP32-C3 / 240 × 320 屏幕。设计页面为 `design-preview/index.html`；本地预览：`python3 -m http.server 8765 --directory design-preview`。

## 操作

- 首页：UP/DOWN 浏览本期和往期，最后一条往期再按 DOWN 回顶部；OK 刷新。长按 UP 或在顶部短按 UP 进入观察，长按 DOWN 进入设置；长按 OK 不触发刷新。
- 观察：默认选中顶部「特码生肖／平特生肖」切换，UP/DOWN 选择分组、排序及生肖行，OK 切换当前选项；长按 OK 回首页。默认按连续未出现期数降序，可切换为近7天出现期数升序。
- 设置：进入时无默认选中。选择 Wi-Fi 管理、重新配网、提醒、音量或亮度；OK 确认。调整音量或亮度时 UP 增加、DOWN 减少、OK 保存；长按 OK 逐级返回。

## 配网与数据

设置中选择「重新配网」。微信打开「蓝牙配网-FoloToy AI PASSPORT」小程序，选择 `BLUFI_FoloPassport`，输入 2.4 GHz Wi-Fi 的名称和密码。成功连接或 180 秒后停止蓝牙；最多 5 组已成功连接的 Wi-Fi 凭据保存在独立 NVS 命名空间。顶部显示当前 SSID 与信号格。

HTTPS GET 验证服务端证书，只声明支持 gzip/identity。平时五分钟同步，北京时间 21:30 至 21:44 每 15 秒同步，失败后退避。SNTP 校时，接口失败保留缓存；无需 Cookie 或 Authorization。

按期号去重、响应时间排序，保留最新 30 期，不把历史 URL 年份当作自然年范围。前六个为平特，第七个为特码。生肖使用每条响应自身的数据，兼容简繁体；平特同一期重复生肖只算一次。近七天包含北京时间今天及此前六个日历日。页面显示历史次数，不自动推荐下一期结果。

设置和记录通过版本化 NVS blob 保存。首次同步静音，后续新期号可播放一次短提示音；先保存已提醒期号，再播放。默认亮度 80%、音量 60%。

设置和离线记录使用 `ninehalf` NVS 命名空间。升级后首次启动会将旧版命名空间中的有效数据复制到当前命名空间；Wi-Fi 凭据仍保存在独立命名空间。

## 验证范围

界面只创建当前可见区域的记录和统计行，仍可浏览完整 30 期；LVGL 内存池为 64 KiB。使用 OFL Noto Sans SC 字体子集，增加文案后通过 `tools/generate_ninehalf_fonts.py` 重新生成。

激活 ESP-IDF 5.5.3 后运行 `./tools/validate.sh`。合并固件 `build/ai-passport-ninehalf-full.bin` 从 `0x0` 写入，内容寻址归档保留匹配的 ELF 和 manifest。合并刷写可能重置保存的设置，未经用户确认不刷写。

主机测试和编译不能确认实机显示完全一致、Wi-Fi/BLE/TLS 同时工作的内存余量、设备接口访问、配网、声音及长时间稳定性；这些需实机验收。
