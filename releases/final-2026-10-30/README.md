# V1.0 · 2026-10-30 发布快照

ESP-IDF 5.5.4 / ESP32-C6，4 MiB Flash，128×32 OLED。对应当前源码清单，历史包不覆盖。

- 应用：1,735,888 字节；2 MiB 程序槽余 361,264 字节（352.8 KiB）。
- SHA-256：`af02f92dcd79f6e30365388f51e416535aad5d694908803c32b3c06cf5d0808c`。
- 布局：factory-2m-usb-v1，应用 0x20000 / 0x200000；仅 USB 更新，不支持 OTA。
- 两处存档：NVS 0x9000 / 0x6000、nvs_sec 0x3b0000 / 0x6000；NVS v7 / storage ABI 1。

包含当前消息、协作、自组网、桌游、宠物、双语与外观功能，以及网络实验和宠物礼物。入口见[使用手册](../../docs/使用手册.md)，测试边界见[发布验证](../../docs/release_2026_10_30_validation.md)。礼物启用 wallet_v1 后不要用旧固件继续养宠物，见[礼物存档说明](../../docs/network_lab_and_gifts.md)。

## 校验及更新

从仓库根目录使用安装了 esptool 的 ESP-IDF Python：

```powershell
python tools/verify.py --release releases/final-2026-10-30
python -m esptool --port COM9 read_mac
python tools/flash_update.py --port COM9 --expect-mac <BASE_MAC> --release releases/final-2026-10-30
```

替换串口及实际六字节 BASE MAC。以上更新只适用于已迁移且被工具识别的 2 MiB 单应用布局，会备份旧应用并回读验证。旧三槽设备必须先整片双读备份、核对并获准迁移；禁止直接刷大应用、单刷分区表或擦除 NVS。详见[发布与构建](../../docs/发布与构建.md)。

| 成套镜像 | 首次安装地址 |
| --- | --- |
| firmware/bootloader.bin | 0x0 |
| firmware/partition-table.bin | 0x8000 |
| firmware/ota_data_initial.bin | 0xf000 |
| firmware/app.bin | 0x20000 |

地址表仅供识别成套产物，维护已有存档设备使用上方安全更新工具，不照搬四段首次安装。manifest.json 与 validation 为配套构建记录。COM9 / COM10 / COM15 已更新并做局部实测；礼物实际转移、物理掉电等未验收，hardware_tested 保持 false。
