<p align="right"><strong>简体中文</strong> · <a href="ninehalf-provision-diagnostic.md">English</a></p>

# 配网诊断 — 2026-10-05

现象：小程序搜索不到设备，设备按 OK 后配网项仍显示「进入」。尚未捕获对应 BLUFI 错误，无法确定无线初始化失败的具体原因。

已确认的代码缺陷：联网任务丢弃配网启动错误，仍报告成功。本版改为报告真实结果并显示失败状态，记录请求派发、可用内存、回调注册、NimBLE、GATT、host task、profile 同步及启动广播阶段。初始化失败后不再向已删除的联网任务发送通知；日志不包含 Wi-Fi 名称或密码。

Build：PASS。完整验证门禁、合并镜像布局及独立归档校验通过。Host tests：PASS，包含配网失败状态的原生渲染、文字宽度与内存检查。Device tests：已刷入且数据哈希校验通过，用户操作复现 NimBLE HCI 初始化失败。Unverified：实际失败阶段、广播发现、手机连接、安全协商、Wi-Fi 连接和记录同步。本版不宣称已修复搜索问题。

合并镜像为 2,093,392 字节，确认后从 `0x0` 写入。代码位于 `main`，未提交。

Full SHA-256: `9370e28c0082151faae2485524de4229e85a0375c914f1ba053c23abc6a450a0`

ELF SHA-256: `199d2c21e7970c59ce09c1e02fd00438e1afc3973a6ec0578e8019b46da6ada7`

Archive: `build/firmware/9370e28c0082151faae2485524de4229e85a0375c914f1ba053c23abc6a450a0`
