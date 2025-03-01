# RV1103VideoTrans - ST7789 Camera Display System

这是一个为Luckfox Pico Mini A/B开发板设计的实时图像显示系统，使用ST7789显示器和CSI摄像头。该系统可以从摄像头捕获图像，进行预处理，并将其实时显示在ST7789 TFT LCD屏幕上。

项目地址: https://github.com/Daofengql/RV1103VideoTrans

## 功能特点

- 使用SPI总线与ST7789显示器通信
- 支持320x240像素的彩色TFT显示
- 实时图像捕获和显示
- 自动白平衡处理
- RGB888到RGB565色彩转换
- 实时帧率计算和显示
- 支持SPI传输的容错和重试机制

## 硬件要求

- Luckfox Pico Mini A/B开发板
- ST7789 TFT LCD显示器 (320x240分辨率)
- CSI接口摄像头
- SPI和GPIO接口连接

## GPIO和屏幕参数详解

### GPIO配置
本系统使用以下GPIO引脚与ST7789显示器通信：

| 引脚功能 | GPIO编号 | 说明 |
|---------|---------|------|
| DC (数据/命令) | GPIO1_C4d (53) | 控制SPI通信时是发送命令还是数据 |
| RES (复位) | GPIO1_C5d (52) | 控制显示器硬件复位 |
| BL (背光) | GPIO (4) | 控制显示屏背光 |

### 显示器参数
- **SPI设备**: `/dev/spidev0.0`
- **SPI速度**: 80MHz (80000000Hz)
- **屏幕分辨率**: 320x240像素
- **色彩格式**: RGB565 (16位色彩)
- **通信模式**: SPI Mode 0

### ST7789初始化序列
程序包含完整的ST7789初始化序列，设置了以下参数：
- 软件复位
- 退出睡眠模式
- 16位色彩模式 (RGB565)
- RGB模式配置
- 伽马校正
- 显示反转设置
- 显示窗口配置 (0-239, 0-319)

## 依赖库

- OpenCV（图像处理）
- 标准C/C++库
- Linux SPI和GPIO接口

## 安装与编译

### 交叉编译环境要求
本项目需要使用uClibc版本的ARMv7交叉编译工具链，而非标准glibc版本。uClibc是一个为嵌入式系统优化的C库，占用空间更小，更适合Luckfox Pico Mini等资源受限的设备。

1. 安装ARMv7 uClibc交叉编译工具链：

```bash
# 注意：需要使用uClibc版本的交叉编译工具链
# 可以从Luckfox官方获取适用于Pico Mini的uClibc工具链
# 或使用buildroot构建uClibc工具链
```

2. 安装OpenCV依赖：

```bash
sudo apt-get install libopencv-dev
```

3. 克隆代码库：

```bash
git clone https://github.com/Daofengql/RV1103VideoTrans.git
cd RV1103VideoTrans
```

4. 编译程序：

```bash
# 使用提供的构建脚本进行交叉编译
./build-linux.sh
```

## 部署和运行

1. 将编译好的程序传输到Luckfox Pico Mini：

```bash
scp -r ./release/* user@luckfox-device:/path/to/destination/
```

2. 在Luckfox Pico Mini上运行程序：

```bash
cd /path/to/destination/
./VT
```

## 自定义配置

如需修改系统参数，请参考以下代码中的常量：

```c
// GPIO和显示器配置
#define GPIO_DC    53   // GPIO1_C4d for DC pin
#define GPIO_RES   52   // GPIO1_C5d for RES pin
#define GPIO_BL    4
#define DISPLAY_SPI_DEVICE   "/dev/spidev0.0"
#define SPI_SPEED 80000000
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define BUFFER_SIZE  (SCREEN_WIDTH * SCREEN_HEIGHT * 2)
#define BLOCK_SIZE   4096
#define MAX_RETRIES  3
```