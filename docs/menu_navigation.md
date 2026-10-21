# FUN / TOOLS 导航整理

2026-10-21：菜单按用途排序，功能、页面 ID、分区和存档格式不变，没有删游戏。自动化脚本从 `firmware/ui/menu_layout.h` 读取顺序，避免继续使用旧下标点错功能。

## FUN：陪伴、单机、附近互动

1. Pet：电子宠物。
2. Tetris portrait：竖屏俄罗斯方块。
3. Snake：贪吃蛇。
4. Answer book：答案之书。
5. Dice：普通骰子。
6. Craps：花旗骰。
7. RPS nearby：附近石头剪刀布。
8. Treasure radar：寻宝雷达。
9. Reaction party：多人反应赛。
10. Number bomb：数字炸弹。
11. Time guess 1-10s：随机秒数盲猜。

## TOOLS：遥控、联网、扫描、诊断

1. Slide remote：PPT 遥控。
2. Media remote：媒体遥控。
3. Camera remote：手机拍照适配。
4. Wi-Fi connect：连接/管理热点。
5. Wi-Fi scanner：附近热点扫描。
6. BLE scanner / RSSI：蓝牙扫描与信号。
7. Memory status：运行内存。

## 主菜单文字

保留 FUN、TOOLS 短标题。FUN 描述为 `Pet, arcade & party games`，TOOLS 描述为 `Remotes, Wi-Fi & scanners`。

长文字仅在选中磁贴内滚动，左右端各停留 900 ms，按时间推进约每 35 ms 一像素。切换磁贴或重新进入主菜单后从开头读起，不覆盖图标或底部页码点。短文字保持静止。

遵守现有 `SETTINGS > Text Scroll` 设置。关闭时改用能放下的短说明 `Pet & party games` 和 `Remote / Wi-Fi`，不强行截掉长说明。列表自身原有的选中行滚动继续保留。

## 验证

- `python tools/test_menu_layout.py`：检查 11 个 FUN 和 7 个 TOOLS 映射完整、标签/路由来自同一配置、滚动暂停/折返/复位、裁切和禁用时短文案。这是生产代码宿主测试。
- `python tools/smoke_menu_layout.py --port COM9`：逐项进入真实 18 个入口并返回，不在游戏/工具内提交动作，不配对/发送遥控按键、不连接陌生 Wi-Fi。扫描页仍会正常执行被动扫描，Pet 正常后台成长。保存真实屏幕与日志到 `build/menu-layout-smoke/`。

### 2026-10-21 COM9 记录

构建/发行校验与烧录哈希通过。应用 1,221,056 字节，比宠物版增加 400 字节，单槽余量 24,128 字节（约 23.6 KiB）。应用 SHA-256 `5c629b0440c678e6fd0ef5b2e956ead994465ce12ac551c5dfa15da3c9003d01`。

实际逐项打开 11 个 FUN、7 个 TOOLS 页面，确认均到达预期页面并返回；俄罗斯方块按自身暂停菜单选择 EXIT 退出，没有误把“返回=暂停”当作离开。最后回到原首页，无崩溃/栈检查错误。

人工查看 FUN/TOOLS 的起始帧和滚动帧，长说明随时间移动，图标和底部页码不被文字覆盖。Pet 仍显示原睡眠幼体（BABY 10m），没有重新生成蛋。此次只验证导航和显示，不代替全部功能重新验收或多人射频测试。
