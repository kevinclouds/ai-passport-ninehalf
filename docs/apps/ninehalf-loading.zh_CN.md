<p align="right"><strong>简体中文</strong> · <a href="ninehalf-loading.md">English</a></p>

# 内容区 loading

用户确认使用整个内容区 loading。首页记录区替换为居中转圈及中文状态，保留顶部状态栏和底部。时间无效时显示正在校时。联网时按 OK 立即进入 loading，忙碌时重复 OK 不叠加刷新。联网任务通过已有 UI 队列发送开始和结束消息，实时及历史请求过程结束后退出。请求失败恢复缓存记录及原有失败标题；长按 OK 仍可打开菜单。loading 不清除记录。

此前 `cbe1bbce` 实机 identity HTTPS 已通过：实时 200、解析 1 条，历史 200、解析 30 条，日志 `sync ok`，启动后约 21 秒完成。新增 loading 镜像已明确授权从 `0x0` 刷入，esptool 的 Flash hash 校验通过，启动 ELF 前缀 `6a25db704` 匹配。180 秒监听已开启；初始日志尚未显示 Wi-Fi 获取 IP，实机视觉验收等待用户操作。

Build：PASS。Host tests：PASS，原生 loading、失败、恢复及原有导航、设置压力验证通过；loading 占用 12,584 字节，保留 64 KiB LVGL 池。中文字体已重新生成并核对覆盖（233 字形／12 尺寸）。Device tests：loading NOT RUN。Unverified：实机转圈、慢请求及失败切换、提醒及反复退出配网。

合并镜像 2,101,616 字节，从 `0x0` 刷入。Full SHA-256 `d17ce335b1a65439a93b042f3c55382b389a94d98398ac5a651001de2a167197`；ELF SHA-256 `6a25db70498b6720f198683ddd9e3cf6ff7caa6dd7676decde7134dee7a3770f`。原生效果 `design-preview/native/loading.png`，预览画廊 `http://127.0.0.1:8765/native.html`。改动在 `main`，未提交。

## 连续旋转修正

默认变弧长 spinner 改为固定 90 度圆弧、1,000 ms 线性旋转、无限循环及零循环等待。中途 render 保留 loading 区和动画对象，仅更新状态文字。原生验证推进 120 × 20 ms，跨越多个周期检查角度步进及中途 render 后 root/arc 对象不变。完整门禁、Host／原生验证及独立归档校验通过。实机流畅度 NOT RUN。Full SHA-256 `b0068579a32a33343f8bb56b93af766e4c59e3049cdeea194fadb48981fcd7a4`，ELF `dbd8de8680ea1ac75a7665c384426216f99d76d4dc92799446724a3b05669394`，合并 2,101,920 字节，从 `0x0` 刷入；尚未刷入，未提交。

连续旋转镜像已明确授权从 `0x0` 刷入。Esptool 的 Flash hash 校验通过，启动 ELF 前缀 `dbd8de868` 与归档一致。180 秒监听已开启，实机连续旋转及刷新后恢复仍等待用户确认；原生动画测试不等于实机验收。

返回首页修复：在观察页启动后台同步，再长按 OK 返回首页，可复现没有新刷新请求却显示整区 loading。将前台 loading 与后台 syncing 分开；已有缓存的自动同步不遮挡记录，仍可浏览。手动 OK 或首次无数据保留 loading，离开首页清除前台 loading 状态。原生回归验证返回不发刷新／不显示转圈、同步中浏览、手动显示已有请求 loading 及动画生命周期。构建/Host/原生/归档通过。候选 adc0af033b9c924658f7dab8b73fae1ff468b64db553219d10ec0675a46491f6 未刷入，实机返回行为待验收，未提交。

经授权分段刷入 adc0af03，保留 NVS。写入校验通过，启动 ELF 前缀 525479b75 匹配。自动联网、实时 HTTPS 200／解析 1 条通过；55 秒窗口中历史接口 HTTP 200，未捕获解析／sync 完成。串口已关闭。返回首页／手动刷新实机验收等待用户反馈，未提交。

用户确认 adc0af03 返回首页直接显示缓存记录，短按 OK 才显示手动刷新 loading，两项实机验收通过。
