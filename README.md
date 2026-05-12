# watch-diy

完全 DIY 的儿童电话手表项目（4G 语音通话 + GPS 定位 + SOS + 微聊 + 拍照），面向 4 岁孩子日常佩戴，对标小天才电话手表 Z 系列基础能力，**不商用、自用**。

## 目录结构

| 目录 | 说明 |
|---|---|
| [`docs/`](docs/) | 需求文档与设计文档（先看这里） |
| [`hardware/`](hardware/) | KiCad 原理图、PCB、3D 表壳模型、BOM |
| [`firmware/`](firmware/) | 手表端固件（ESP-IDF v5.x + FreeRTOS + LVGL） |
| [`server/`](server/) | 后端（Go + Gin + EMQX + PostgreSQL + MinIO） |
| [`app/`](app/) | 家长 APP（Flutter，iOS + Android） |
| [`tools/`](tools/) | 自签证书生成、固件烧录、批量配置等脚本 |

## 文档入口

| 编号 | 文档 | 说明 |
|---|---|---|
| 00 | [愿景与范围](docs/00-vision.md) | V1/V2 边界、不做清单、关键决策 |
| 01 | [功能性需求](docs/01-requirements-functional.md) | 按模块组织的 user story 与验收标准 |
| 02 | [非功能性需求](docs/02-requirements-nonfunctional.md) | 续航、防水、性能、安全、隐私 |
| 03 | [硬件设计](docs/03-hardware-design.md) | BOM、原理图模块、PCB 叠层、电源树 |
| 04 | [固件架构](docs/04-firmware-architecture.md) | Task 划分、内存预算、状态机、AT 驱动 |
| 05 | [后端与 APP 架构](docs/05-cloud-and-app.md) | API、MQTT topic、数据模型、APP 信息架构 |
| 06 | [项目执行清单（P0–P5）](docs/06-project-checklist.md) | 分阶段 step-by-step，`- [ ]` 打勾跟踪进度 |

## 关键技术决策（速览）

- 主控：ESP32-S3-WROOM-1U-N16R8
- 4G + GNSS：移远 EC800 系列（Cat.1 数据 + GNSS；**V1 语音走 APP**，VoLTE pstn 不强制，见 `docs/00-vision.md` §4.0）
- 屏幕：1.43" 圆形 AMOLED 466×466 SH8601 + FT3168 触摸（QSPI）
- SIM：Nano SIM 卡座 + 消费级运营商儿童副卡
- 后端：Ubuntu 22.04 单机 Docker Compose（裸 IP + 自签证书 + 高位端口）
- 家长 APP：Flutter 3.x（一码 iOS + Android）

## 项目原则

1. **安全 > 功能 > 性能 > 美观**：儿童佩戴的电子设备，所有取舍优先看安全
2. **数据自主**：所有用户数据存自己服务器，不接入任何第三方分析/广告 SDK
3. **可维护性 > 极致**：单台调试方便比量产美感更重要（V1 范围）
4. **公开设计、私有数据**：硬件/固件/服务端代码 MIT，但 CA 私钥与设备数据严格私有
