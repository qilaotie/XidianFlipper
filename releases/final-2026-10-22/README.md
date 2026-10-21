# V1.0 最终版镜像包 · 2026-10-22

ESP-IDF 5.5.4 / ESP32-C6 / 本项目 4 MiB 分区布局。使用前阅读[发布与构建](../../docs/发布与构建.md)和[最终版说明](../../docs/最终版发布说明.md)。不是 V1.5，不适配其他硬件。

| 文件 | 地址 | 用途 |
| --- | --- | --- |
| firmware/bootloader.bin | 0x0 | 引导 |
| firmware/partition-table.bin | 0x8000 | 分区表 |
| firmware/ota_data_initial.bin | 0xf000 | 初始启动选择数据 |
| firmware/app.bin | 0x20000 | 应用 |

应用 SHA-256：`b0e2cb23354e88d5b2dde47f66113245a1c013d8a9c3dd6d157d08bfd5bbf33d`。

各文件哈希、大小与适用信息见 manifest.json；源码记录与构建报告在 validation。此包来自同一次成功构建，不含设备 NVS、密码、串口日志或本机缓存。

已有同布局设备先核对当前启动槽，再选应用更新流程；不要随意写初始启动数据或整片擦除。首次安装与已有设备更新不是同一操作。

从仓库根目录核验此快照（需安装适用 Python 环境）：

```powershell
python -c "import sys;sys.path.insert(0,'tools');from verify import verify;from pathlib import Path;verify(Path('releases/final-2026-10-22'))"
```

修改编译输入后必须重新构建，再发布新快照，不覆盖冻结原版。此快照已通过清单核验，局部实机与宿主测试范围见发布说明；不宣称整机全量验收。
