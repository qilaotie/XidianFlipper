# 半步小岛 · XidianFlipper

网站：<https://qilaotie.github.io/XidianFlipper/>

固件和使用手册在同仓库 `main` 分支；网站唯一维护源码在本 `gh-pages` 分支，本地为 `halfstep/WEB/`，不复制到固件工作目录。

## 当前进度

- GitHub Pages 静态网站：游戏排行榜、宠物展示柜、连接指南；真实空状态，不注入演示成绩。
- Cloudflare Workers + D1：接收和查询数据；无付费订阅、无付费域名购买步骤。
- **设备固件尚未接入同步，旧固件连接 Wi-Fi 不会自动上传。** 当前程序分区剩余约 3.2 KiB，需要测量 HTTPS 接入后的体积，不修改既有分区或静默删除游戏。
- 网站插画为代码绘制示意，不是设备截图。

## 运行与验证

Node 24+：`npm test`；本地网页可运行 `python -m http.server 8080 --bind 127.0.0.1`。

本机 npm 损坏时可使用独立 pnpm；不需要更改系统 Node 配置。工具版本固定在 `package.json`。

后端：`npx wrangler@4.151.3 login`，`npx wrangler@4.151.3 d1 migrations apply xidianflipper-community --remote`，`npx wrangler@4.151.3 deploy`。不要在不确认目标的情况下另建重复数据库。

前端：完成测试后将本分支推送 `origin/gh-pages`。GitHub Pages 来源应为 `gh-pages /`。`node tools/github-pages.mjs` 只读检查；加 `--enable` 只在尚未启用 Pages 时创建配置，不覆盖已有不同来源。该脚本通过操作系统 Git 凭据助手在内存中鉴权，绝不输出密钥。

## HTTP API v1

公开地址见 `config.js`，所有请求必须使用 HTTPS。

- `GET /health`：服务存活。
- `GET /v1/leaderboard?game=snake|tetris`：最高分前 100，分数降序，同分按达成时间、设备匿名 ID 升序。蛇的分数为食物数量，方块为引擎总分。
- `GET /v1/pets`：最近更新的 100 台设备。
- `POST /v1/sync`：设备首次上传建立记录，之后同一匿名 ID 必须带原凭证。最多每 60 秒一次；成绩只增不减。
- `DELETE /v1/sync`：同样鉴权，JSON `{ "id": "..." }`；隐藏宠物、清空该设备云端分数。设备需同时关闭自动同步，否则后续主动上传会重新公开。保留匿名 ID 和凭证哈希，避免被别人抢占；公开读取缓存最多延迟约 30 秒。

写接口需 `Content-Type: application/json` 和 `Authorization: Bearer <64位小写十六进制随机凭证>`。设备匿名 ID 为 32 位小写十六进制真随机值，**不是 MAC**。首次生成后必须持久化；不要对每次请求重新生成，更不能在源码里共享同一凭证。

上传格式：

```json
{
  "id": "<32 hex>",
  "nickname": "Player_1",
  "pet": {
    "age": 1801, "form": 1, "coins": 30, "dex": 1, "decor": 0,
    "fullness": 80, "mood": 90, "energy": 100, "clean": 70,
    "sleeping": false
  },
  "scores": [
    {"game": "snake", "score": 4, "progress": 7},
    {"game": "tetris", "score": 120, "progress": 1}
  ]
}
```

`age` 是累计养成秒数，不是墙上时钟。昵称仅支持 ASCII 字母数字、空格及 `_.-`，1–16 字符；昵称可重复，身份依赖随机 ID。蛇 `progress=score+3`；方块 progress 为累计消行纪录，分数和消行分别为本机纪录，不必来自同一局。未知字段丢弃，密码、Wi-Fi SSID、IP、MAC 不写入 D1。HTTP 请求经过 Cloudflare 基础设施，其基础访问处理不等于匿名于平台。

错误：400 无效数据；401 凭证不匹配；403 不允许的网页来源；413 超过 2048 字节；415 非 JSON；429 限频；503 暂不可用或满员。设备应退避，不因失败清除本地存档。上传不能阻塞 UI，也不能在任意后台线程直接读取 UI 专属的宠物状态。

## 安全、隐私和免费边界

- 第一版为开放自报社区，不是账号认证或强防作弊服务。任何人可模拟新设备；随机凭证只保护已有设备记录不被其他人覆盖。未来开放大规模使用前应加邀请/人机验证或所有权绑定。
- 写 IP 限频约 10 次/分钟，读约 120 次/分钟；Cloudflare 限频按边缘节点，非全球严格配额。单设备同步另有 D1 时间检查。异常 IP 只用于限频键，不作为应用字段存储。
- 首版最多 2000 个设备 ID（包括隐藏记录）；防止数据库无限增长，但不是防刷注册的完整方案。
- 公开读取缓存 30 秒，索引查询；不存照片、历史轨迹或逐局日志，不启用 Worker 请求日志。
- Workers Free / D1 免费额度用完可能停服，设备离线功能不应受影响。**不要把 Workers 升级为付费套餐**；代码限频不是付费套餐的硬金额上限。本次没有购买或升级套餐，套餐详情可在账号 Billing 中核对。
- Cloudflare OAuth 凭证留在官方 Wrangler 的用户配置目录，GitHub 密钥由 OS 凭据助手管理，均不在本仓库。不要发布 `.env`、`.dev.vars`、`.wrangler` 或测试私有数据。

## 验证边界

`tests/worker.test.mjs` 使用生产 Worker 和真实内存 SQLite，验证字段白名单、首次注册、错凭证拒绝、最高分不回退、限频、容量和隐藏删除；限频绑定、边缘缓存在该测试中为替身。

页面和真实 Cloudflare API 可单独在线验证，但不等于 ESP32 上传已通过。设备端尚未部署同步固件，不能声称设备端已验收。
