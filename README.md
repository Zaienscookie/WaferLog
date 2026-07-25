# WaferLog | 硅笺

WaferLog 是一个面向 Tuya T5AI-Board 的嵌入式智能记录终端项目

它提供手写笔记、页面管理、录音入口、联网状态、蓝牙设备连接、日历和后续 AI 分析能力等。使用 LVGL 构建界面

## 项目定位

WaferLog 的主要使用方式是：

1. 在设备上手写记录笔记、想法和灵感等等一切你想要的，everything
2. 通过录音入口记录语音内容
3. 保存笔记页面
4. 通过 Wi-Fi 上传笔记或录音 data
5. 由服务器调用多模态大模型进行分析。
6. 将总结、知识点、Idea 扩展和 RAG 关系数据返回到设备或 Web 服务
7. 在横屏模式下作为桌面电子日历使用 (当然也可以做 L2D model)
8. ......


## 当前开发

- LVGL 图形界面
- 竖屏 `360 × 640`
- 横屏 `640 × 360`
- 首页、笔记页、日历页三项导航
- 手写笔记结构
- 横线、方格、点阵和空白纸张
- 笔宽调整
- 多种笔色 (各种 color , pick by yourself)
- 橡皮擦等 tools
- 清空当前页面 ( debug@localhost#: clear )
- 本地保存
- Wi-Fi 上传状态
- Wi-Fi、蓝牙和录音服务状态
- 亮色、暗色和多套主题色
- 多语言入口
- 横屏桌面日历

### 当前正在完善

- 暗色模式下的深色手写画布
- 深色界面中文字对比度
- 多语言正文切换
- 手写事件状态和笔触稳定性
- Wi-Fi 模块的多项功能
- 蓝牙设备搜索列表
- 真实 T5AI-Board 服务层
- 服务器上传接口 api
- 多模态 AI 分析接口 api

### 首页

首页包含：

- 时间和日期
- 语言按钮
- 外观按钮
- 横竖屏切换按钮
- Hero 快速开始区域
- Wi-Fi 卡片
- Bluetooth 卡片
- 最近笔记卡片
- Wi-Fi、蓝牙、上传和电量状态条
- 底部导航
- ......

### 手写笔记页

手写页包含：

- 当前页码和总页数
- 上一页、下一页
- 工具栏展开和收起
- 保存
- 上传
- 纸张类型选择
- 四种笔色
- 笔宽调整
- 橡皮擦
- 清空当前页面
- 全屏手写画布
- 画布专注模式
- 底部导航
- ......

一些修复的笔记 (碎碎念)：手写数据通过单画布缓冲区加笔画段记录的方式，避免为每一页都长期占用完整 RGB565 缓冲区，方便后续迁移到 T5AI-Board

### 日历页

日历页支持：

- 横屏桌面日历
- 当前时间
- 当前日期
- 天气占位信息
- 今日议程
- 月历
- 当前日期高亮
- 月份前后切换
- 日历优先针对横屏模式设计
- ......

### 语言弹层

计划支持：

- English
- 中文
- 日本語
- Français
- Русский


### 外观弹层

外观设置包含：

- 亮色模式
- 暗色模式
- 青绿色主题
- 紫色主题
- 黄色主题
- 粉色主题

暗色模式需要同时改变 (考虑到的问题)：

- 页面背景
- 卡片背景
- 文字颜色
- 边框颜色
- 纸张背景
- 纸张线条
- 手写墨水对比度
- 连接弹层和输入框

## 手写交互设计

手写输入分为三个阶段：

```text
PRESSED
    记录起点，不新增笔画

PRESSING
    坐标发生变化后，新增一段笔画

RELEASED / PRESS_LOST
    清除当前笔画状态
```

伪压感只用于轻微调整笔宽 (是的，我们想在一定程度上，有限的板子上玩出更多花样)：

- 慢速移动略微变粗
- 快速移动略微变细

## Wi-Fi 与蓝牙

### Wi-Fi

计划提供：

- 开启或关闭 Wi-Fi
- 搜索附近网络
- 显示网络名称
- 显示信号强度
- 显示加密状态
- 点击网络连接
- 手动添加隐藏网络
- 输入密码
- 显示或隐藏密码
- 连接成功后显示当前网络
- 断开当前网络

### 蓝牙

计划提供：

- 开启或关闭蓝牙
- 搜索附近设备
- 显示设备名称
- 显示信号强度
- 连接设备
- 断开设备

## 服务层

UI 不直接依赖 TuyaOS 的底层 API，统一通过 `src/hal/waferlog_services.*` 访问服务。

当前服务接口包括：

```c
bool waferlog_recording_start(void);
void waferlog_recording_stop(void);
bool waferlog_recording_is_active(void);

bool waferlog_wifi_connect(const char * ssid, const char * password);
void waferlog_wifi_disconnect(void);
bool waferlog_wifi_is_connected(void);
bool waferlog_note_upload(void);

bool waferlog_ble_enable(void);
void waferlog_ble_disable(void);
bool waferlog_ble_is_enabled(void);
```

联网搜索接口将继续保持在服务层：

```c
bool waferlog_wifi_scan(void);
uint32_t waferlog_wifi_scan_count(void);
const char * waferlog_wifi_scan_ssid(uint32_t index);
int32_t waferlog_wifi_scan_signal(uint32_t index);
bool waferlog_wifi_scan_secured(uint32_t index);

bool waferlog_ble_scan(void);
uint32_t waferlog_ble_scan_count(void);
const char * waferlog_ble_scan_name(uint32_t index);
int32_t waferlog_ble_scan_signal(uint32_t index);
```

## 目标硬件

目标开发板：

```text
Tuya T5AI-Board
T5-E1-IPEX
```

板卡相关功能规划：

- LCD 图形
- 板载麦克风录音
- Wi-Fi 联网
- 蓝牙连接
- TF 卡或本地存储
- ......

## 目录结构

```text
WaferLog/
├─ CMakeLists.txt
├─ README.md
├─ docs/
├─ src/
│  ├─ main.c
│  ├─ waferlog_ui.c
│  ├─ waferlog_ui.h
│  ├─ fonts/
│  │  ├─ waferlog_font_14.c
│  │  └─ waferlog_font_16.c
│  └─ hal/
│     ├─ hal.c
│     ├─ hal.h
│     ├─ waferlog_services.c
│     └─ waferlog_services.h
├─ lvgl/
└─ SDL2-2.30.1/
```

## Windows 模拟器

### 构建

在项目根目录执行：

```powershell
$env:Path='D:\.Desktop\rebuild\lvgl_dev_env\cmake-4.1.2-windows-x86_64\bin;D:\.Desktop\rebuild\lvgl_dev_env\mingw64\bin;'+$env:Path
cmake --build build --target waferlog --parallel 4
```

### 运行

```powershell
$env:Path='D:\.Desktop\rebuild\lvgl_dev_env\lv_port_pc_vscode-master\bin;D:\.Desktop\rebuild\lvgl_dev_env\mingw64\bin;'+$env:Path
Start-Process .\build\bin\waferlog.exe
```

## 后续产品链路

设备端负责：

- 手写采集
- 录音采集
- 本地保存
- 网络连接
- 上传任务状态
- AI 分析结果展示
- ......


服务器负责：

- 文件接收
- 语音转写
- 图像和手写内容识别
- 多模态内容总结
- 知识点提取
- Idea 扩展
- 节点图生成
- RAG 关系数据生成
- 用户数据存储和同步
- ......


当前仓库主要聚焦设备端 LVGL 界面和本地模拟器，服务器和 AI 服务保持后续独立设计。

## 相关文档

- [Tuya T5-E1-IPEX 开发板资料](https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj)

- [LVGL CMake INTERFACE_INCLUDE_DIRECTORIES 报错修复方案]
(https://blog.csdn.net/reload111/article/details/162833555?spm=1001.2014.3001.5501)
什么？你怎么知道网上很多人问但是没人解决？所以我写了解决方案
