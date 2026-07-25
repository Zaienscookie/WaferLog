# WaferLog Web 前端重构与部署指导

## 1. 文档目标

本文用于指导重构 WaferLog 的 Web 前端，并将其部署到现有服务器。

## 1.1 交给其他 agent 的一次性执行要求

将本文交给其他 agent 时，要求其把本节到文末作为一个完整任务一次执行，不要只生成方案或只改一个页面。agent 开始前必须先读取当前 `cloud-web` 的目录、路由、服务端 API、数据库结构、现有设置页和启动脚本，然后在现有代码基础上完成整套前端重构与服务端配套。

一次任务必须包含：

1. 保留并验证现有笔记、关系、时间线、日历、问答、设备音频上传和设备笔记上传功能。
2. 完成 OpenAI 兼容接口设置页、TTS 设置页、配置保存、脱敏读取、连通性测试和试听功能。
3. 完成服务端配置存储、密钥加密、接口校验、权限边界、SSRF 防护、日志脱敏和错误处理。
4. 完成前端 API 封装、加载/保存/测试/失败/未保存状态，以及 UTF-8 字符显示检查。
5. 执行依赖安装、前端生产构建、服务端冒烟测试，并修复构建或测试发现的问题后再次验证。
6. 按本文的 systemd、Nginx 和 HTTPS 章节准备可部署产物，并给出实际启动命令、访问地址和验收结果。

agent 不得删除或改名现有 T5AI-Board 设备接口，不得把 API Key 放进前端源码、浏览器存储或构建产物，不得用示例代码替代实际可运行实现。除非明确收到拆分任务指令，否则应在一次任务中完成上述全部内容，并在最后报告修改文件、测试命令、测试结果、未完成项和需要人工确认的部署步骤。

目标包括：

- 保留现有笔记、主题、关系、时间线、日历、问答和设置功能。
- 在设置页直接编辑 OpenAI 兼容接口配置。
- 在设置页直接编辑 OpenAI 兼容 TTS 接口配置。
- 支持测试接口连通性、保存配置和显示当前生效状态。
- 保持 T5AI-Board 上传笔记和音频的设备接口不变。
- 前端和服务端能够由同一个域名对外提供服务。

## 2. 当前技术基础

当前项目位于 `cloud-web`，主要技术栈为：

- 前端：React 18、TypeScript、Vite、React Router。
- 服务端：Node.js、Express。
- 数据：SQLite。
- 本地开发端口：前端 `5173`，服务端 `2001`。
- 生产模式：Node.js 在 `3737` 同时提供 Web 和 API；设置 `COMPAT_PORT=80` 后，额外兼容现有 T5AI-Board 的 `80` 端口上传请求。

当前主要路由：

| 路径 | 页面 |
| --- | --- |
| `/` | 档案首页 |
| `/notes` | 笔记列表 |
| `/notes/:id` | 笔记详情 |
| `/topics` | 主题管理 |
| `/timeline` | 时间线 |
| `/calendar` | 日历 |
| `/ask` | 知识问答 |
| `/settings` | 设置 |

## 3. 推荐架构

浏览器不应直接携带 API Key 请求 OpenAI 或 TTS 服务。推荐流程如下：

```text
浏览器设置页
    -> WaferLog 配置 API
    -> 服务端安全保存配置
    -> WaferLog AI/TTS 代理层
    -> OpenAI 兼容服务
```

设置仍然由用户在前端页面直接修改，但密钥只提交给 WaferLog 服务端。

禁止以下实现：

- 将 API Key 写入前端源码。
- 将 API Key 写入 Vite 的 `VITE_*` 环境变量。
- 将 API Key 写入 `localStorage`、`sessionStorage` 或 Cookie。
- 将完整 API Key 通过配置查询接口返回给浏览器。
- 由浏览器直接请求第三方模型服务。

## 4. 设置页信息结构

建议将 `/settings` 拆分为以下区域：

1. 设备状态。
2. OpenAI 兼容接口。
3. TTS 接口。
4. 数据和导出。
5. 外观与纸张偏好。
6. 回收站。

设置页保持工作台风格，不使用营销式页面布局。配置项使用输入框、下拉菜单、开关和明确的测试按钮。

## 5. OpenAI 兼容接口设置

建议支持以下字段：

| 字段 | 说明 | 示例 |
| --- | --- | --- |
| `enabled` | 是否启用外部模型 | `true` |
| `baseUrl` | OpenAI 兼容接口根地址 | `https://api.openai.com/v1` |
| `apiKey` | 接口密钥 | 只允许写入，不允许明文读取 |
| `model` | 文本分析模型 | 由用户填写或选择 |
| `visionModel` | 图片和手写内容分析模型 | 可与文本模型相同 |
| `transcriptionModel` | 语音转写模型 | OpenAI 兼容 ASR 模型 |
| `timeoutMs` | 请求超时 | `60000` |
| `temperature` | 生成随机度 | `0.2` |
| `maxTokens` | 最大输出长度 | `4096` |

页面交互要求：

- API Key 输入框默认使用密码模式。
- 已保存密钥显示为 `sk-****abcd` 一类掩码。
- 密钥输入框留空时，保存操作不覆盖已有密钥。
- 提供显示或隐藏本次输入内容的按钮。
- 提供“测试连接”按钮。
- 测试期间按钮显示加载状态并禁止重复点击。
- 测试结果显示延迟、模型名称和错误原因。
- 保存成功后显示最后更新时间。

## 6. TTS 接口设置

TTS 配置应独立于文本模型配置，允许使用不同服务商。

建议字段：

| 字段 | 说明 | 示例 |
| --- | --- | --- |
| `enabled` | 是否启用 TTS | `true` |
| `baseUrl` | TTS 服务根地址 | OpenAI 兼容地址 |
| `apiKey` | TTS 密钥 | 只允许写入 |
| `model` | TTS 模型 | 由用户填写 |
| `voice` | 默认音色 | 由服务端支持列表决定 |
| `format` | 输出格式 | `mp3`、`wav` 或 `pcm` |
| `speed` | 播放速度 | `1.0` |
| `sampleRate` | PCM 采样率 | `16000` 或 `24000` |
| `timeoutMs` | 请求超时 | `60000` |

页面应提供一段短文本作为试听内容。点击“试听”后，由服务端调用 TTS 接口并返回音频，浏览器使用原生音频控件播放。

## 7. 前端类型定义

建议新增以下类型：

```ts
export interface OpenAISettings {
  enabled: boolean
  baseUrl: string
  apiKeyConfigured: boolean
  apiKeyMasked: string
  model: string
  visionModel: string
  transcriptionModel: string
  timeoutMs: number
  temperature: number
  maxTokens: number
  updatedAt: string | null
}

export interface TtsSettings {
  enabled: boolean
  baseUrl: string
  apiKeyConfigured: boolean
  apiKeyMasked: string
  model: string
  voice: string
  format: 'mp3' | 'wav' | 'pcm'
  speed: number
  sampleRate: number
  timeoutMs: number
  updatedAt: string | null
}
```

保存请求单独定义写入类型，允许包含可选的 `apiKey`：

```ts
export interface OpenAISettingsInput {
  enabled: boolean
  baseUrl: string
  apiKey?: string
  model: string
  visionModel: string
  transcriptionModel: string
  timeoutMs: number
  temperature: number
  maxTokens: number
}
```

## 8. 服务端配置接口

建议增加以下接口：

| 方法 | 路径 | 用途 |
| --- | --- | --- |
| `GET` | `/api/settings/providers` | 获取脱敏后的全部配置 |
| `PUT` | `/api/settings/openai` | 保存 OpenAI 配置 |
| `POST` | `/api/settings/openai/test` | 测试模型接口 |
| `PUT` | `/api/settings/tts` | 保存 TTS 配置 |
| `POST` | `/api/settings/tts/test` | 测试并返回试听音频 |
| `DELETE` | `/api/settings/openai/key` | 清除 OpenAI 密钥 |
| `DELETE` | `/api/settings/tts/key` | 清除 TTS 密钥 |

配置查询响应示例：

```json
{
  "openai": {
    "enabled": true,
    "baseUrl": "https://api.openai.com/v1",
    "apiKeyConfigured": true,
    "apiKeyMasked": "sk-****abcd",
    "model": "model-name",
    "visionModel": "vision-model-name",
    "transcriptionModel": "transcription-model-name",
    "timeoutMs": 60000,
    "temperature": 0.2,
    "maxTokens": 4096,
    "updatedAt": "2026-07-25T12:00:00.000Z"
  },
  "tts": {
    "enabled": true,
    "baseUrl": "https://api.openai.com/v1",
    "apiKeyConfigured": true,
    "apiKeyMasked": "sk-****wxyz",
    "model": "tts-model-name",
    "voice": "default",
    "format": "mp3",
    "speed": 1,
    "sampleRate": 24000,
    "timeoutMs": 60000,
    "updatedAt": "2026-07-25T12:00:00.000Z"
  }
}
```

保存时，如果请求没有 `apiKey` 字段，服务端保留旧密钥。如果传入非空 `apiKey`，服务端替换旧密钥。

## 9. 服务端配置存储

推荐在 SQLite 中新增 `provider_settings` 表：

```sql
CREATE TABLE provider_settings (
  provider TEXT PRIMARY KEY,
  enabled INTEGER NOT NULL DEFAULT 0,
  configJson TEXT NOT NULL,
  encryptedApiKey TEXT,
  updatedAt TEXT NOT NULL
);
```

密钥至少应使用服务端主密钥加密后存储。主密钥通过环境变量提供：

```text
WAFERLOG_SETTINGS_SECRET=长度足够的随机字符串
```

生产环境不要将主密钥提交到 Git。服务端启动时如果主密钥不存在，应拒绝保存新的 API Key，并打印清晰错误。

## 10. AI 服务层改造

当前 `server/src/ai.js` 是规则引擎兜底层。重构后建议保留现有函数签名，在内部增加 Provider 分发：

```text
业务服务
    -> AI Provider
        -> OpenAI Compatible Provider
        -> Rule Provider
```

调用顺序：

1. 读取 OpenAI 配置。
2. 配置启用且完整时调用外部模型。
3. 外部模型超时或失败时记录错误。
4. 根据设置决定回退到规则引擎或直接返回错误。
5. 将模型、耗时和调用结果写入服务端日志。

图片、手写笔画、文本和录音转写应统一整理为服务端内部的分析请求，不让页面直接拼接模型协议。

## 11. 前端 API 封装

在 `web/src/api.ts` 中增加：

```ts
settings: () => req<ProviderSettings>('/api/settings/providers'),
saveOpenAI: (body: OpenAISettingsInput) =>
  req<OpenAISettings>('/api/settings/openai', {
    method: 'PUT',
    body: JSON.stringify(body),
  }),
testOpenAI: (body: OpenAISettingsInput) =>
  req<ProviderTestResult>('/api/settings/openai/test', {
    method: 'POST',
    body: JSON.stringify(body),
  }),
saveTts: (body: TtsSettingsInput) =>
  req<TtsSettings>('/api/settings/tts', {
    method: 'PUT',
    body: JSON.stringify(body),
  }),
```

TTS 试听接口返回音频时，不使用通用 JSON 请求函数，直接读取 `Blob`：

```ts
const response = await fetch('/api/settings/tts/test', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify(payload),
})

if (!response.ok) throw new Error(await response.text())
const audioUrl = URL.createObjectURL(await response.blob())
```

页面卸载或生成新试听地址时，调用 `URL.revokeObjectURL`。

## 12. 设置页状态设计

每个 Provider 区域至少处理以下状态：

- 首次加载。
- 加载失败。
- 未配置。
- 已配置但未启用。
- 已启用。
- 表单有未保存修改。
- 保存中。
- 测试中。
- 测试成功。
- 测试失败。

保存和测试是两个独立操作。测试按钮可以使用当前表单内容，但不能自动保存未确认的修改。

## 13. 权限和安全

部署到公网前至少完成：

- 为设置接口增加登录验证。
- 为修改和清除密钥的接口增加管理员权限。
- 使用 HTTPS。
- 限制设置接口的请求频率。
- 限制 `baseUrl` 可访问的协议，默认只允许 `https://`。
- 防止服务端请求本机、内网和云元数据地址，避免 SSRF。
- 服务端日志不得打印 API Key、Authorization 请求头或完整配置对象。
- TTS 试听文本设置长度上限。
- 模型测试请求设置超时和响应大小上限。

开发阶段可以暂时不接入完整用户系统，但不要因此把密钥存入浏览器。

## 14. 开发流程

安装依赖：

```powershell
cd cloud-web
npm ci
```

同时启动服务端和前端：

```powershell
npm run dev
```

浏览器访问：

```text
http://127.0.0.1:5173/settings
```

前端开发服务器会将 `/api` 代理到 `http://localhost:2001`。

完成后执行：

```powershell
npm run build
npm run smoke
```

## 15. 生产构建

在服务器项目目录执行：

```bash
cd /opt/waferlog/cloud-web
npm ci
npm run build
```

构建产物位于：

```text
cloud-web/web/dist
```

生产环境由 Node.js 直接托管该目录，并同时提供 `/api` 接口。

启动服务：

```bash
PORT=3737 COMPAT_PORT=80 WAFERLOG_SETTINGS_SECRET='服务端随机密钥' npm start
```

## 16. systemd 示例

创建 `/etc/systemd/system/waferlog.service`：

```ini
[Unit]
Description=WaferLog Web
After=network.target

[Service]
Type=simple
WorkingDirectory=/opt/waferlog/cloud-web
Environment=NODE_ENV=production
Environment=PORT=3737
Environment=COMPAT_PORT=80
EnvironmentFile=/etc/waferlog/waferlog.env
ExecStart=/usr/bin/npm start
Restart=always
RestartSec=3
User=waferlog

[Install]
WantedBy=multi-user.target
```

环境文件 `/etc/waferlog/waferlog.env`：

```text
WAFERLOG_SETTINGS_SECRET=使用随机生成的长密钥
```

设置权限：

```bash
sudo chown root:waferlog /etc/waferlog/waferlog.env
sudo chmod 640 /etc/waferlog/waferlog.env
sudo systemctl daemon-reload
sudo systemctl enable --now waferlog
```

## 17. 端口与反向代理

直接部署时，Node.js 服务使用 `3737` 提供 Web/API，并使用 `80` 作为板端兼容入口。若前面已有 Nginx，可只代理 Web/API 端口，板端兼容入口仍需保留到 `80`：

```nginx
server {
    listen 80;
    server_name your-domain.example;

    client_max_body_size 8m;
    root /opt/waferlog/cloud-web/web/dist;

    location /api/ {
        proxy_pass http://127.0.0.1:3737;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

配置域名后使用 Certbot 或其他证书方案启用 HTTPS。

## 18. 与 T5AI-Board 的兼容要求

以下设备接口不要在前端重构时删除或改名：

- `POST /api/device/audio`
- `GET /api/device/audio/:id`
- `POST /api/device/notes`
- `GET /api/device/notes/:id`
- `GET /api/device/sync`

Web 前端只消费这些数据，不应改变板端上传协议。若未来增加图片二进制上传，应新增独立接口，例如：

```text
POST /api/device/images
GET /api/device/images/:id
```

不要让图片上传改动破坏现有笔画 JSON 和音频 PCM 上传。

## 19. 推荐实施顺序

1. 修复现有前端文件的字符编码，统一为 UTF-8。
2. 新增服务端配置表和密钥加密模块。
3. 新增配置读取、保存、清除和测试接口。
4. 在 `api.ts` 中封装设置接口。
5. 重构 `SettingsPage.tsx`，加入 OpenAI 和 TTS 表单。
6. 接入 OpenAI Provider，保留规则引擎兜底。
7. 接入 TTS Provider 和试听接口。
8. 增加权限、SSRF 防护和日志脱敏。
9. 执行前端构建、API 冒烟测试和部署测试。
10. 最后再接入真实 API Key。

## 20. 验收标准

- 设置页可以读取脱敏配置。
- 用户可以修改 Base URL、模型、音色和请求参数。
- 用户可以更新或清除 API Key。
- 页面刷新后配置仍然存在。
- 浏览器存储和前端构建产物中不存在 API Key。
- OpenAI 测试能返回模型和耗时。
- TTS 测试能在页面内播放试听音频。
- OpenAI 服务不可用时，规则引擎仍可工作。
- T5AI-Board 原有笔记和音频接口保持可用。
- 生产环境由 HTTPS 提供服务。
- 服务重启后配置和数据不丢失。
