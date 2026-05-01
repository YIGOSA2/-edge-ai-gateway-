# Edge AI Gateway 项目文档

[English](PROJECT_DOC_EN.md)

## 项目概述

在 RK3506 Linux 嵌入式开发板上搭建多传感器采集系统，通过本地 AI 模型实时判断设备状态异常，结合 MQTT 上报和 Web 看板，实现完整的边缘智能网关。

## 硬件平台

| 项目      | 型号/配置                             |
| ------- | --------------------------------- |
| 开发板     | 万象奥科 RK3506 EVM                   |
| 温湿度传感器  | AHT30 (I2C, 0x38)                 |
| 加速度传感器  | MPU6500 (I2C, 0x68)               |
| 开发主机    | Ubuntu 22.04                      |
| 交叉编译工具链 | arm-buildroot-linux-gnueabihf-gcc |
| 云服务器    | MQTT Broker (mosquitto 2.0.11)    |

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
 │   running: 运行标志                        │
 └───────────────────────────────────────────┘
      │                     │                │
 ┌────▼────┐         ┌─────▼─────┐    ┌─────▼──────┐
 │AHT30    │         │SQLite DB  │    │MQTT Broker │
 │MPU6050  │         │sensor.db  │    │(云服务器)   │
 │(I2C硬件)│         │(NAND存储) │    │  Broker    │
 └─────────┘         └───────────┘    └──────┬─────┘
                                            │
                                     ┌──────▼─────┐
                                     │Web 看板     │
                                     │dashboard.html│
                                     └────────────┘
```

## 文件结构

```
src/
├── main.c              # 主程序入口，线程创建 + 信号处理
├── collector.h         # 共享数据结构 (sensor_data_t, shared_data_t)
├── sensor_thread.c     # 传感器采集线程 (100ms采样)
├── storage_thread.c    # SQLite存储线程 (1s写入)
├── ai_thread.c         # AI推理线程 (方差振动检测 + 模型温湿度检测)
├── mqtt_thread.c       # MQTT上报线程 (5s上报JSON)
├── aht30.c / aht30.h   # AHT30温湿度传感器驱动
├── mpu6050.c / mpu6050.h # MPU6050加速度传感器驱动
├── db_storage.c / db_storage.h # SQLite数据库操作模块
├── ai_infer.h          # AI推理函数 (Python训练生成)
└── Makefile            # 交叉编译脚本

ai/
├── label_data.py       # 数据标注脚本
├── train_model.py      # 模型训练脚本
├── sensor_data.csv     # 原始采集数据
└── labeled_data.csv    # 标注后数据

web/
└── dashboard.html      # Web实时看板
```

## 线程详解

### 1. 采集线程 (sensor_thread.c)

- 每 100ms 读取 AHT30 温湿度 + MPU6050 三轴加速度
- 获取单调时间戳
- 加锁写入 `g_shared.data`

### 2. 存储线程 (storage_thread.c)

- 初始化 SQLite 数据库 (`/root/sensor.db`)
- 每 1 秒读取共享数据，插入数据库
- 每 60 条打印进度

### 3. AI 推理线程 (ai_thread.c)

- 维护 5 个采样点的滑动窗口
- 计算加速度方差，方差 > 1,000,000 判定为振动异常
- 调用 `ai_predict()` 检测温湿度异常 (temp>35°C, humi>65%)
- 连续 3 次异常才报警，结果写入 `g_shared.ai_status`

### 4. MQTT 线程 (mqtt_thread.c)

- 每 5 秒读取共享数据和 AI 状态
- 构建 JSON 消息，调用 `mosquitto_pub` 发送到云服务器
- Topic: `edge-gateway/sensor`

## 数据库表结构

```sql
CREATE TABLE readings (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp  INTEGER NOT NULL,
    temp       REAL,
    humi       REAL,
    vib_x      INTEGER,
    vib_y      INTEGER,
    vib_z      INTEGER,
    ai_result  INTEGER DEFAULT 0
);
```

## AI 模型

### 训练流程

1. 板子采集正常数据 30 分钟 + 异常数据 15 分钟 (捂热、摇晃、哈气)
2. 从 SQLite 导出 CSV
3. Python 标注 (基于特征阈值自动标注)
4. scikit-learn 训练 Random Forest (50棵树, 最大深度6)
5. 导出决策树为 C 语言 if-else 代码

### 训练结果

- 数据量: 2696 条 (正常 2493, 异常 203)
- 准确率: 99.8%
- 特征重要性: vib_z(51.3%) > vib_x(31.2%) > vib_y(8.9%) > humi(5.3%) > temp(3.4%)

### 实际部署方案

训练后发现单棵决策树的振动规则与倾斜数据冲突 (倾斜时单轴值也很大)，因此实际部署采用**双路检测**:

- **振动检测**: 滑动窗口方差分析 (5个采样点, 阈值1,000,000)
- **温湿度检测**: AI模型导出的温度/湿度规则

## MQTT 通信

### JSON 消息格式

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

### 配置

- Broker: 自建 MQTT 服务器（修改 `src/mqtt_thread.c` 中的 `MQTT_BROKER_IP`）
- 端口: 1883 (TCP)
- WebSocket: 9001 (Web看板用)
- Topic: `edge-gateway/sensor`
- QoS: 1
- 上报间隔: 5秒

## Web 看板

- 纯前端 HTML + JavaScript
- 通过 MQTT WebSocket 连接云服务器
- 实时显示温度、湿度、三轴振动
- 异常时红色闪烁警告
- 事件日志记录最近100条

## 编译部署

```bash
# 编译
make clean && make

# 部署到板子
make deploy

# 板子上运行
./gateway

# 优雅退出 (Ctrl+C)
```

## 开发过程中遇到的问题及解决方案

### 1. 传感器 WHO_AM_I 返回 0x70 而非 0x68

模块实际使用 MPU6500 芯片而非 MPU6050，引脚兼容、寄存器基本一致，代码无需修改。

### 2. AI模型倾斜误报

模型训练数据中没有"倾斜但静止"的数据，导致将倾斜时的大单轴值误判为振动。
**解决**: 使用滑动窗口方差分析检测振动，不受倾斜影响。

### 3. MQTT与AI线程检测结果不一致

`sensor_thread` 每100ms覆盖整个 `g_shared.data`，将AI线程写入的 `ai_status` 冲掉。
**解决**: 将 `ai_status` 从 `sensor_data_t` 移到 `shared_data_t`，传感器线程无法触及。

### 4. Web看板 WebSocket 连接失败 (HTTPS限制)

页面通过 HTTPS 访问时，浏览器禁止发起 `ws://` 不安全连接。
**解决**: 云服务器 mosquitto 开启 9001 端口的 WebSocket 监听，通过 HTTP 访问看板。

### 5. mosquitto_pub 缺少动态库

板子上单独拷贝 `mosquitto_pub` 二进制后报 `libmosquitto.so.1` 缺失。
**解决**: 同时拷贝动态库文件并执行 `ldconfig`。
