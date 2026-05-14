# watch-diy 固件（ESP-IDF）

与 [docs/06-project-checklist.md](../docs/06-project-checklist.md) **P1** 对齐；**P0 硬件未到货时**可先在本目录完成 **编译链验证**（与板子无 USB 连接也可 `build`）。

## 前置条件

- 已安装 **ESP-IDF**（推荐安装器所选 **v6.x 稳定版**，或 v5.2+）。
- Windows：**固件开发以命令行 `idf.py` 为主**。使用 **「ESP-IDF PowerShell」** 或 **「ESP-IDF CMD」**（快捷方式指向你本机真实安装目录，例如 `D:\Espressif`）进入本目录，以便 `IDF_PATH` 已配置；**不依赖** VS Code 乐鑫插件也可完整编译与烧录。

## 首次编译（不接开发板）

```powershell
cd d:\repository\watch-diy\firmware
idf.py set-target esp32s3
idf.py build
```

成功则生成 `build/watch_diy_fw.bin`（名称以 CMake `project()` 为准）。

## 分区表（阶段 0.3）

- 文件：**`partitions_watch.csv`**（工程根 `firmware/` 下，与 `CMakeLists.txt` 同级）。  
- 配置：`sdkconfig` / `sdkconfig.defaults` 中 **`CONFIG_PARTITION_TABLE_CUSTOM=y`**，**`CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_watch.csv"`**。  
- **当前用途**：`nvs`、`phy_init`；应用从 **`factory`** 启动；**`idf.py flash`** 默认写入 **`factory`** 分区。  
- **OTA 占位（未接代码）**：`otadata`、`ota_0`、`ota_1` 与 `factory` 同大小（各 **2 MiB**），便于后续按 ESP-IDF **App OTA** 接入（对齐 [07-开发路线图](../docs/07-development-roadmap.md) 阶段 5.2）。在实现 OTA 前勿随意擦除 **`otadata`**，除非按官方 OTA 文档操作。  
- 表内英文注释与 [04-固件架构](../docs/04-firmware-architecture.md) 后续 OTA 章节可交叉引用。

## 命令速查（阶段 0.4）

在 **`firmware/`** 目录下（路径示例：`d:\repository\watch-diy\firmware`）：

| 步骤 | 命令 |
|------|------|
| 首次 / 换芯片目标 | `idf.py set-target esp32s3` |
| 编译 | `idf.py build` |
| 仅烧录 | `idf.py -p COMx flash` |
| 仅串口监视 | `idf.py -p COMx monitor` |
| 烧录并监视 | `idf.py -p COMx flash monitor` |

将 **`COMx`** 换为设备管理器中的端口（如 `COM3`）。退出监视：**`Ctrl+]`**。  

同样内容在 [06-项目执行清单](../docs/06-project-checklist.md) 的 **P1.0** 一段有对应说明，便于检索。

## `main/` 目录约定（阶段 0.2）

| 子目录 | 职责 |
|--------|------|
| `board/` | 引脚与 BSP（如 `bsp_board.h`、`bsp_lcd.*`） |
| `ui/` | LVGL 界面模块 |
| `services/` | 平台服务（如 NVS 早期初始化 `svc_system.*`） |
| `app/` | 应用编排（`watch_app_start()`） |
| `app_main.c` | 仅入口，调用 `watch_app_start()` |

**若出现 `UnicodeDecodeError` / `charmap` / `kconfgen` 失败**：Windows 下 `sdkconfig.defaults` 须为 **纯英文注释**（勿写中文），已如此维护；请先删 `sdkconfig`（若已生成）并执行 `idf.py fullclean` 后重编。

**若日志显示 target 为 esp32**：必须先执行 `idf.py set-target esp32s3`，勿用 esp32。

## 烧录与监视（硬件到后）

USB 连接 **ESP32-S3 圆屏开发板**，确认设备管理器中 COM 口。与上表一致，常用一条命令：

```powershell
cd d:\repository\watch-diy\firmware
idf.py -p COMx flash monitor
```

也可分步执行 **`idf.py -p COMx flash`** 与 **`idf.py -p COMx monitor`**。退出监视：`Ctrl+]`。

## VS Code + Espressif 插件（可选）

**可不装、可不用。** 若扩展 2.x / EIM 与你的工作流不合，**只用「ESP-IDF PowerShell」+ `idf.py` 即可**，与本仓库说明一致。

ESP-IDF 工程根目录是 **`firmware/`**。请用 **「文件 → 打开文件夹」** 打开 **`d:\repository\watch-diy\firmware`**，不要只打开外层 `watch-diy`，否则插件会在仓库根找 `sdkconfig` 并报错。

### 重要：扩展 2.0.2+ 与「手写 `idf.espIdfPath`」

从 **ESP‑IDF Extension v2.0.2** 起，官方**取消了**通过设置项配置 `idf.espIdfPath`、`idf.toolsPath` 的主要路径，**改为 [Espressif Installation Manager（EIM）](https://docs.espressif.com/projects/idf-im-ui/en/latest/)** 在全局状态里登记环境。因此：

- 日志里 **`C:\Espressif\tools\eim_idf.json`** 是扩展**固定会先找**的 EIM 元数据路径（Windows 上常见默认盘符），**不等于**把你的 IDF 改成了 C: 盘；你装在 **D:\Espressif** 时，这里仍会报「文件不存在」，属预期噪声。
- 在 `settings.json` 里写 **`idf.espIdfPath` / `idf.toolsPath` / `idf.pythonBinPath`**，在 **2.1.x** 里往往**不再驱动「Project setup」**，编辑器里可能显示灰色；**以 Doctor 为准**——若仍是空，说明扩展没从 EIM/全局状态里拿到环境。

**你可选的做法：**

1. **推荐（与当前 D: 经典安装并存）**：继续用 **「ESP‑IDF PowerShell / CMD」** + `idf.py`，不依赖 VS Code 里 Doctor 全绿。
2. **想用扩展 2.x 管路径**：安装/运行 **EIM**，在其中登记或安装一套 ESP‑IDF，让扩展能生成/读到 **`eim_idf.json`**（路径仍可能是 `C:\Espressif\tools\`，由 EIM 决定）。
3. **想继续「纯手写路径」**：把 **Espressif IDF** 扩展 **降级到 2.0.2 之前的版本**（例如 **1.8.x**），再使用各教程里的 `idf.espIdfPath` 写法。

本仓库 **`firmware/.vscode/settings.json`** 目前只保留扩展仍常用的项（如 **`idf.port`**、**`idf.openOcdConfigs`** for ESP32‑S3）；COM 口请按本机修改。

若需在用户级消除 Git 报错，在 VS Code **用户** `settings.json` 中设置：**`"idf.gitPath": "C:/Program Files/Git/cmd/git.exe"`**（路径按本机 `where git` 调整），**勿用**未展开的 `${env:programfiles}`。

## 下一步（到货后 / 并行后续）

1. **P1.1 显示**：按微雪 Wiki 添加 **显示驱动 + LVGL 9**（可用组件管理器：`idf.py add-dependency "lvgl/lvgl^9"`，具体以微雪例程为准）。
2. **P1.2 蜂窝**：UART 接 4G 板，实现 AT 队列与 PDP（见 [docs/04-firmware-architecture.md](../docs/04-firmware-architecture.md)）。

## 目录说明

| 路径 | 说明 |
|------|------|
| `main/app_main.c` | 入口；当前仅 NVS + 日志 |
| `sdkconfig.defaults` | 默认 Kconfig（S3 + Octal PSRAM、LVGL、**自定义分区表**） |
| `partitions_watch.csv` | 分区表：`factory` + OTA 占位（见文内「分区表」节） |
| `build/`、`managed_components/` | 本地生成，已 `.gitignore` |
