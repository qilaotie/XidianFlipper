# 半步电台 · V1.0 功能扩展版

当前发布：2026-10-23，包含宠物用户命名、贪吃蛇三地图与清晰边框、方块/蛇存档和最高纪录，以及自愿开启的 HTTPS 云端分享。应用 1,527,808 字节，2 MiB 应用槽剩 556 KiB。

**旧三槽设备必须先备份并迁移，不能直接烧入大应用或单刷分区表。** 两处 NVS 地址不变；迁移后仅支持 USB 更新，不支持 OTA。详见[分区迁移](docs/partition_migration.md)。

基于 ESP32-C6 的旋钮交互小设备：附近消息、摩斯学习、多人游戏、电子宠物，以及蓝牙遥控和 Wi-Fi 工具。

独立固件工程，目标 **ESP-IDF 5.5.4 / ESP32-C6 / 4 MiB Flash / 128×32 OLED**。整理日期：2026-10-23。维护版固件字符串仍为 `1.0.0-noautosleep`；区分构建请看发行清单与 SHA-256，不把本扩展版误称为 V1.5。

项目仓库：[qilaotie/XidianFlipper](https://github.com/qilaotie/XidianFlipper)。当前镜像见 [releases/final-2026-10-23](releases/final-2026-10-23/README.md)；[10 月 22 日旧包](releases/final-2026-10-22/README.md)仅作历史快照。在线社区：[半步小岛](https://qilaotie.github.io/XidianFlipper/)。

## 功能概览

- 附近通信：快捷短语、最长 128 个 ASCII 字符的机身编辑、字符表情、频道、好友定向消息、收件箱与发送状态。
- 摩斯与学习：附近实时摩斯、自由练习、听音选字、跟练和听写。
- FUN 共 11 个入口：宠物、竖屏俄罗斯方块、贪吃蛇、答案之书、骰子、花旗骰、附近石头剪刀布、寻宝雷达、多人反应赛、数字炸弹、随机 1–10 秒盲猜。
- 宠物：用户命名、照顾与进化、需求提醒、自主动作、七个小游戏、金币商店、背包、装饰、图鉴、双机拜访与对战。
- 单机游戏：方块/蛇最高纪录与继续上局，蛇的经典、立柱、关卡门三图，游戏成绩可为宠物赚金币。
- TOOLS 共 8 项：PPT、媒体、拍照遥控，Wi-Fi 连接、云同步、Wi-Fi 扫描、BLE/RSSI 扫描、运行内存状态。

附近消息与游戏采用自定义 BLE 广播，无需路由器、SIM 卡或手机；不是运营商短信，也不是标准 Bluetooth Mesh。Wi-Fi 用于连接管理和可选云分享，不承载附近消息与联机对战。云分享默认关闭，开启后公开宠物摘要和两款游戏最高分，可在设备上撤下。

## 文档导航

| 文档 | 内容 |
| --- | --- |
| [功能简介](docs/功能简介.md) | 功能、人数、适用场景与能力边界 |
| [完整使用手册](docs/使用手册.md) | 机身按键、消息、游戏、工具和故障排查 |
| [宠物专用手册](docs/宠物功能使用手册.md) | 养成、七游戏、金币、商店、图鉴和联机 |
| [原版对比](docs/原版对比与新增功能.md) | 保留与新增功能、验收口径 |
| [PPT 展示提纲](docs/PPT展示提纲.md) | 展示文案、现场操作和答辩 |
| [发布与构建](docs/发布与构建.md) | 编译、核验、烧录和数据保护 |
| [最终版发布说明](docs/最终版发布说明.md) | 镜像指纹、空间和验证范围 |
| [2 MiB 分区迁移](docs/partition_migration.md) | 私有整片备份、USB 迁移、数据保护与恢复边界 |
| [半步小岛](docs/cloud_community.md) | 网页排行榜、宠物展示柜与设备接入进度 |

专题：[菜单](docs/menu_navigation.md) · [编辑器](docs/message_compose.md) · [方块](docs/tetris.md) · [蛇](docs/snake.md) · [多人游戏](docs/party_extra_games.md) · [工具](docs/wireless_tools.md) · [拍照](docs/camera_remote.md) · [Wi-Fi](docs/wifi_connect.md) · [宠物联机](docs/pet_connection_games.md)。专题中的旧构建数据保留为历史验收，最终构建以当前清单为准。

## 快速开始

1. 烧录前阅读[发布与构建](docs/发布与构建.md)，确认硬件和启动槽，不整片擦除已有设备。
2. 首页短按进入菜单，转动选择、短按确认，独立按键 BACK 返回；长按含义因页面而异。
3. 多台演示前统一版本，频道消息选择相同房间；HID 遥控期间电台通信暂停。
4. 宠物入口 `FUN > Pet`。每项目说明完整看三次后跳过，`Pet > HOW TO` 随时回看。
5. 遥控时进入对应 TOOLS 页面，再在主机蓝牙设置配对 `Halfstep Remote`。
6. 宠物首次起名，之后 `Pet > NAME` 改名，最多 12 个英文/数字/符号。蛇新局先选图，旧局可继续。
7. 云功能先连可上网的 2.4 GHz Wi-Fi，再去 `TOOLS > Cloud sync` 自愿开启。宠物本机名字与云端设备昵称目前是两套名称，详见[云端说明](docs/cloud_community.md)。

## 开发与编译

直接在本目录打开 VS Code。工程自带 firmware、main、components 与配置，不依赖另一版本源码；ESP-IDF 工具链需另行安装。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
python tools/verify.py
```

默认工具链路径为 `C:\esp\v5.5.4\esp-idf` 与 `C:\Espressif`，其他电脑通过 `-IdfPath`、`-IdfToolsPath`、`-IdfPythonEnvPath` 指定。核验应使用同一 ESP-IDF Python 环境，不混用旧缓存或其他 SDK 产物。

输出在 `build/release/`：配套镜像、manifest.json、源码记录和验证报告。build 缓存不进 Git。`releases/original/` 为冻结基准，不覆盖。

`启动网页.cmd` / `open_web.ps1` 仅是相邻 `../WEB/` 的可选入口，本仓库不含该网页。没有 WEB 不影响独立编译和 esptool 烧录。

## 当前镜像快照（2026-10-23）

| 项目 | 数值 |
| --- | --- |
| 应用镜像 | 1,527,808 字节 |
| 单应用槽 | 2,097,152 字节（2 MiB） |
| 单槽剩余 | 569,344 字节（556 KiB） |
| 分区与存储 | factory-2m-usb-v1；NVS v7、storage ABI 1 |
| 宠物联机载荷 | 版本 2，双方须同版 |

应用 SHA-256：`21c39ff927a790cb9d25f497df559b704af222e0c0e4a81414797ad242a916f3`。这是应用槽余量，不是 RAM 或整片 Flash 的空闲容量。

## 验证与限制

当前镜像通过 5.5.4 构建、清单核验和 COM9 应用回读，实机验证命名及三地图、昵称与地图重启恢复。云同步上传、撤下、重分享是同日上一云版本的 COM9 实测；COM14 已验证上一云版本的保数据迁移。历史 COM9/COM10 双机记录不代表最新镜像全量验收，详见[发布说明](docs/最终版发布说明.md)。清单 `hardware_tested: false` 保留。

- RSSI 只表示信号冷热，不给方向或精确距离。
- 拍照/PPT/媒体遥控取决于主机系统、应用和配对状态。
- Wi-Fi 获取 IPv4 不等于互联网连通。
- 消息不是加密私聊，游戏没有密码学防作弊；Wi-Fi 凭据未启用 NVS 加密，不输入真实敏感信息。
- 没有 IR、NFC/RFID、Sub-GHz 或 V1.5 OTA，不是完整 Flipper Zero 替代品。

第三方组件沿用各目录原有许可证，例如 `components/u8g2/LICENSE`。本次整理未替第三方代码重新授权。
