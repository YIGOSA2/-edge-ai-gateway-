# Edge AI Gateway

基于 RK3506 嵌入式 Linux 平台的边缘 AI 智能网关，集成多传感器采集、本地 AI 异常检测、SQLite 本地存储、MQTT 云端上报和 Web 实时看板。

## 系统架构

```
┌──────────────────────────────────────────────────────┐
│                      main.c                          │
│    初始化传感器 → 创建4个线程 → 等待 → 清理退出       │
└─────┬──────────┬──────────┬──────────┬──────────────┘
      │          │          │          │
 ┌────▼───┐ ┌───▼────┐ ┌───▼────┐ ┌───▼────┐
 │采集线程 │ │存储线程│ │AI线程  │ │MQTT线程│
 │100ms/次│ │1s/次   │ │1s/次   │ │5s/次   │
 └────┬───┘ └───┬────┘ └───┬────┘ └───┬────┘
      │          │          │          │
 ┌────▼──────────▼──────────▼──────────▼────┐
 │          shared_data_t (互斥锁保护)        │
 │   data: sensor_data_t (传感器最新值)       │
 │   ai_status: AI推理结果 (0=正常, 1=异常)   │
 └───────────────────────────────────────────┘
      │                     │                │
 ┌────▼────┐         ┌─────▼─────┐    ┌─────▼──────┐
 │AHT30    │         │SQLite DB  │    │MQTT Broker │
 │MPU6050  │         │sensor.db  │    │  (云服务器) │
 │(I2C硬件)│         │(本地存储)  │    └──────┬─────┘
 └─────────┘         └───────────┘           │
                                       ┌─────▼──────┐
                                       │ Web 看板    │
                                       │dashboard   │
                                       └────────────┘
```

## 硬件

| 项目 | 型号 | 接口 | I2C 地址 |
|------|------|------|---------|
| 开发板 | 万象奥科 RK3506EVM | — | — |
| 温湿度传感器 | AHT30 | I2C | 0x38 |
| 加速度传感器 | MPU6050 / MPU6500 | I2C | 0x68 |

## 功能特性

- **多传感器采集** — AHT30 温湿度 + MPU6050 三轴加速度，100ms 采样周期
- **本地 AI 推理** — 双路异常检测：滑动窗口方差振动检测 + 随机森林温湿度规则检测
- **本地 SQLite 存储** — 1 秒间隔持久化所有传感器读数
- **MQTT 云端上报** — 5 秒间隔 JSON 格式上报至云服务器
- **Web 实时看板** — 纯前端 MQTT WebSocket 看板，异常红色闪烁告警
- **多线程架构** — 采集/存储/AI/MQTT 四线程解耦，互斥锁保护共享数据

## 项目结构

```
edge-ai-gateway/
├── src/
│   ├── main.c              # 主入口，线程创建与信号处理
│   ├── collector.h         # 共享数据结构定义
│   ├── sensor_thread.c     # 传感器采集线程
│   ├── storage_thread.c    # SQLite 存储线程
│   ├── ai_thread.c         # AI 推理线程
│   ├── mqtt_thread.c       # MQTT 上报线程
│   ├── aht30.c / .h        # AHT30 驱动
│   ├── mpu6050.c / .h      # MPU6050/6500 驱动
│   ├── db_storage.c / .h   # SQLite 操作模块
│   ├── ai_infer.h          # AI 推理函数（Python 训练生成）
│   ├── test_all.c          # 传感器独立测试程序
│   └── Makefile            # 交叉编译脚本
├── ai/
│   ├── label_data.py       # 规则标注脚本
│   ├── train_model.py      # 随机森林训练 + C 代码导出
│   ├── sensor_data.csv     # 原始采集数据
│   └── labeled_data.csv    # 标注后数据
├── web/
│   └── dashboard.html      # 实时监控看板
├── 01-i2c-setup.md         # 教程：I2C 验证与传感器接线
├── 02-sensor-reading.md    # 教程：C 语言读取传感器
├── 03-multithread-storage.md # 教程：多线程框架与 SQLite
├── 04-ai-model.md          # 教程：数据采集与模型训练
└── 05-mqtt-web.md          # 教程：MQTT 上报与 Web 看板
```

## 快速开始

### 环境要求

- **交叉编译工具链**：`arm-buildroot-linux-gnueabihf-gcc`（Buildroot SDK）
- **目标板依赖**：`libpthread`、`libsqlite3`、`mosquitto_pub`
- **Python 训练环境**：`numpy`、`pandas`、`scikit-learn`
  注：本次项目中使用的是开发板的i2c1结点。

### 编译与部署

```bash
cd src

# 修改 Makefile 中的 SDK 路径（指向你的 Buildroot SDK）
# SDK_DIR = /path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot

# 编译
make clean && make

# 部署到开发板（修改 Makefile 中的 BOARD_IP）
make deploy

# 在板子上运行
./gateway

# 退出 (Ctrl+C)
```

### AI 模型训练

```bash
cd ai

# 1. 标注数据
python3 label_data.py

# 2. 训练模型并导出 C 代码到 src/ai_infer.h
python3 train_model.py
```

## AI 异常检测

采用双路检测策略，互补覆盖不同异常场景：

| 检测路径 | 方法 | 触发条件 |
|----------|------|----------|
| 振动检测 | 5 点滑动窗口方差分析 | 三轴总方差 > 1,000,000 |
| 温湿度检测 | 随机森林导出规则 | 温度 > 35.05°C 或湿度 > 65% |

连续 3 次异常判定后才触发告警，避免误报。

训练数据：2696 条样本（正常 2493，异常 203），模型准确率 99.8%。

## Web 看板

直接用浏览器打开 `web/dashboard.html`，通过 MQTT WebSocket 连接云服务器实时显示：

- 温度、湿度、三轴振动数值
- AI 检测状态（正常 / 异常闪烁告警）
- 最近 100 条事件日志

> 注意：看板通过 `ws://` 连接，需通过 HTTP 访问页面，不能通过 HTTPS。

## MQTT 协议

```json
{
    "timestamp": 15112565,
    "temp": 27.72,
    "humi": 41.11,
    "vib_x": -11416,
    "vib_y": -112,
    "vib_z": 11792,
    "ai_status": 0,
    "ai_label": "normal"
}
```

- **Topic**：`edge-gateway/sensor`
- **QoS**：1
- **上报间隔**：5 秒
