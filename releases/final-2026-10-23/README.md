# V1.0 当前发布包 · 2026-10-23

ESP-IDF 5.5.4 / ESP32-C6 / 4 MiB Flash / 128×32 OLED。源码位于仓库根目录，四份配套镜像均来自同一次成功构建。不是 V1.5，不适用于其他硬件。

**factory-2m-usb-v1 单应用布局：旧三槽设备必须先双重备份并迁移，不能单刷分区表或直接写入大 app。迁移后仅支持 USB 更新，没有 OTA app 槽。** 详见[安全烧录](../../docs/发布与构建.md)与[迁移](../../docs/partition_migration.md)。不要整片擦除，不复制其他设备 NVS。

| 文件 | 地址 | 用途 |
| --- | --- | --- |
| firmware/bootloader.bin | 0x0 | 配套引导，维护设备默认不写 |
| firmware/partition-table.bin | 0x8000 | 2 MiB 单应用分区表，迁移时最后写 |
| firmware/ota_data_initial.bin | 0xf000 | 清除旧 OTA 启动选择，不等于支持 OTA |
| firmware/app.bin | 0x20000 | 1,527,808 字节应用，2 MiB 槽余 556 KiB |

应用 SHA-256：`21c39ff927a790cb9d25f497df559b704af222e0c0e4a81414797ad242a916f3`。全部镜像大小与哈希见 manifest.json，源码文件记录、SDK 配置与构建报告见 validation/。不包含设备运行数据、密码、上传凭证或串口日志。

从仓库根目录使用 ESP-IDF Python 核验：

```powershell
python tools/verify.py --release releases/final-2026-10-23
```

源码改动后旧包将不再匹配源码记录，应重新构建，勿跳过核验。更新/迁移工具同样支持 `--release releases/final-2026-10-23`，按安全烧录文档指定实际串口和 BASE MAC，不强刷未知布局。

本版新增命名、三地图，保留游戏存档、最高纪录与可选 HTTPS 云同步。当前 COM9 有命名/三图和重启恢复实机证据；云完整链路、COM14 迁移为同日先前云版本实测，未把它们当作当前全量回归；断电恢复、所有手机/网络及四台射频没有全量实测。详见[发布说明](../../docs/最终版发布说明.md)，清单 hardware_tested: false 保留。

releases/original 与 final-2026-10-22 保持冻结历史，不得把这些旧包与当前分区混用。
