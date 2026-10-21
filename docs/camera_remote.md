# 手机拍照遥控适配

入口：`TOOLS → Camera remote`（与另外两个遥控器相邻）。仍使用 `Halfstep Remote` BLE HID，不是 Wi-Fi 相机控制，也不会自动打开相机或验证拍摄结果。

## 使用

1. 设备进入 Camera remote，手机蓝牙连接 Halfstep Remote；手机打开系统相机、解锁并保持拍照模式在前台。
2. 默认 `VOL+ 120ms`。短按旋钮只发送一次“音量＋按下—松开”，观察手机是否拍照。
3. 旋转只切换映射，不发按键：`VOL+ → VOL- → ENTER → SPACE`；依次尝试适合手机和应用的方式。
4. 长按只切换按键持续时间：120 → 200 → 80 → 120 ms，不发送快门。太短无响应可试长一点；若连拍或触发视频，改回 80 ms。
5. BACK 退出并恢复电台通信。

默认 120 ms 是兼容性尝试，不代表所有手机都需要该时长。没有自动连发、自动回退或同时发送多种键，避免一次操作拍多张。按下成功只提示 `KEY SENT - CHECK PHONE`，表示 GATT 提交成功，并不代表手机已拍照。

## 为什么需要独立模式

原 PPT 短按为键盘 Space，媒体模式短按为 Consumer Play/Pause；这两种都不是通用的相机快门命令。新模式默认单发 Consumer Volume Increment，另外提供 Volume Decrement、Keyboard Enter 和 Space 供兼容性测试。按键持续时间独立可调，PPT/媒体仍保留原 45 ms。

HID Report Map、Report ID、GATT 服务和设备名不变，避免破坏已验证的 PPT 配对。若只是连接异常，可在手机蓝牙设置删除旧 Halfstep Remote 后重配；不要求每次升级都删除配对记录。

## 手机侧注意

- 苹果系统相机官方文档说明可以用实体音量键拍照，但这不能证明每个蓝牙 HID 或第三方相机都接受相同事件：<https://support.apple.com/en-au/guide/iphone/iph263472f78/ios>。
- 部分安卓相机有音量键用途设置。例如三星提供拍照/录像、缩放、调音量等选项；若遥控只调音量或缩放，请先选择拍照/录像：<https://www.samsung.com/us/support/answer/ANS10001353/>。
- 先确认手机实体音量键能在当前相机中拍照，再测试蓝牙。ENTER/SPACE 仅是备选映射，不承诺系统相机支持。
- 若屏幕显示 `WAITING: MEDIA REPORT`，表示手机尚未订阅多媒体报告；可尝试切到 ENTER/SPACE，或删除配对后重连。相反键盘报告未订阅时可尝试 VOL+/VOL-。
- 摄影测试需反馈手机型号、系统版本、相机应用，以及“无反应/只调音量/连拍/录像”等具体表现。

## 验证范围

- `tools/test_camera_remote.c`：生产映射、报告字节及持续时间边界。
- `python tools/test_hid_pulse.py`：生产发送/松开函数，覆盖 120 ms 与原 45 ms、忙碌保护、零报告、重试、断线和加密/订阅要求。
- `python tools/test_tool_reports.py`：Report Map 和广播内容未变。
- `python tools/smoke_wireless_tools.py --port COM9 --camera --out build/camera-smoke`：实机页面、映射和时长选择、配对广播、退出后其他遥控页面。脚本不发送拍照按键，以免自动触发用户手机。

手机实际拍摄成功与否必须由目标手机验收，上述测试不能代替。
