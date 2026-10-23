# 半步小岛 · XidianFlipper

网站：<https://qilaotie.github.io/XidianFlipper/>

固件和使用手册在同仓库 `main` 分支；网站唯一维护源码在本 `gh-pages` 分支，本地为 `halfstep/WEB/`，不复制到固件工作目录。

## 当前进度

- GitHub Pages 静态网站：游戏排行榜、宠物展示柜、连接指南；真实空状态，不注入演示成绩。
- Cloudflare Workers + D1 保存数据；免费 Pages API 入口通过内部 Service binding 调用同一后端，未购买域名或升级套餐。
- **COM9 已实测设备直连同步、移除和重新分享。** 主程序槽经备份迁移至 2 MiB；云同步应用 1,525,280 字节，余量 571,872 字节（约 558.5 KiB），未删除游戏。旧固件仅连 Wi-Fi 不会上传；旧分区设备必须先迁移，不能直接刷入大应用。
- 同步固件及完整迁移说明当前在本地 V1.0 工作区；远端 main/final-2026-10-22 仍是历史固件包，尚未自动替换，不要当成新同步版。
- 网站插画为代码绘制示意，不是设备截图。

## 运行与验证

Node 24+：`npm test`；本地网页可运行 `python -m http.server 8080 --bind 127.0.0.1`。

本机 npm 损坏时可使用独立 pnpm；不需要更改系统 Node 配置。工具版本固定在 `package.json`。

后端：`npx wrangler@4.151.3 login`，`npx wrangler@4.151.3 d1 migrations apply xidianflipper-community --remote`，`npx wrangler@4.151.3 deploy`。不要在不确认目标的情况下另建重复数据库。

备用 API 入口：进入 `api-gateway/`，运行 `npx wrangler@4.151.3 pages deploy public --project-name xidianflipper-api --branch main --no-bundle`。只发布明确的 public 目录，不发布仓库根目录。网关无依赖、仅转发 /health 与 /v1/* 到固定 COMMUNITY Service binding，不接受任意目标 URL；不新建第二份 D1。原 workers.dev 在实测网关 DNS/TCP 路径不可用，设备使用 `https://xidianflipper-api.pages.dev`，仍校验 HTTPS 证书和主机名。

## 设备操作简表

先在 `TOOLS > Wi-Fi connect` 连网，再进 `TOOLS > Cloud sync`。转动在四项中切换：第一项长按开/关分享（首次默认 OFF）；第二项短按立即同步；第三项长按关闭并撤下云端展示；第四项短按进入 Wi-Fi 设置。成功显示 SYNC OK / AUTO 15M。关闭只停止后续上传，不会自动撤下已经公开的内容；移除不删本地存档，离线移除意图重启保留。Wi-Fi 自动重连沿用用户原设置，未开启时重启后需手动 Connect saved。网页缓存约 30 秒。

每台设备独立随机身份与凭证先保存再上传，所有者在 UI 线程生成快照，网络在后台串行运行；不上传密码、SSID、MAC、私信、好友或完整棋盘。未知/损坏身份记录不自动覆盖。失败退避，读取纪录不加载游戏或重新发金币。

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

页面/API 测试与设备实机测试分别记录；COM9 的真实同步、移除、同一身份重分享已验证，但没有实机坏证书、拔线或断电故障注入覆盖。

### 2026-10-23 验证记录

- GitHub Pages API 状态 `built`，公网页面 HTTP 200，HTTPS 启用。
- 真实 Workers `/health` HTTP 200，读取接口正常。
- 真实 HTTPS 上传、错凭证 401、同设备冷却 429、隐藏成功。自动测试临时 `API_CHECK` 档案无游戏成绩，结束后按其精确 ID 清理，公开档案数量为 0。
- 生产 SQL 内存 SQLite 测试 2/2 通过；浏览器验证真实空榜、双榜切换、390px 无横向溢出、昵称文本安全渲染、状态条和断网重试。浏览器虚拟档案仅在请求拦截中返回，从未上传。
- 浏览器检查：安装 Playwright 后 `node tools/check-browser.mjs`；可用 `PLAYWRIGHT_MODULE` 指向已有模块路径，`PREVIEW_URL` 指向公网或本地预览。不把机器私有路径写入脚本。
- 发布 Git 操作显式使用 `-c http.sslVerify=true`。机器已有的全局关闭证书校验设置未被本项目修改；请自行审查该既有设置。
- 付费套餐没有开通或升级。OAuth 无订阅读权限，不能据此宣称已读取账单；用户可在 Cloudflare Billing 核对 Free 套餐。

### 同日后续：设备接入验证

- COM9 新布局正常开机，宠物与两游戏菜单可访问；应用更新前后两个 NVS 和分区表回读一致。
- 真实 CHYI 宠物上传：公开随机 ID 前缀 644e3d；蛇最高 4 食物/长度 7，方块 2114 分/10 行，来自设备真实纪录，没有测试分数注入。
- 正常菜单移除后展示柜为空；重新分享恢复同一随机 ID 和两个纪录，设备确认 SYNC OK / AUTO 15M。
- 生产 JSON、云状态机、游戏只读摘要和菜单宿主回归通过；RTOS/HTTP 替身覆盖不冒充实机网络故障验收。
- 网站 SQL/网关共 3 项 Node 测试通过；网关没有任意 URL 代理能力。
- 免费入口与后端仍受账号额度限制，不应理解为无限容量或无限请求。CLI 默认额外生成的未使用 Worker 副本已清理，真正的 Pages 网关、原 Worker 和唯一 D1 保留。
