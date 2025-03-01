#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <fcntl.h>
#include <iostream>

// 系统IO相关  
#include <sys/ioctl.h>

// SPI相关
#include <linux/spi/spidev.h>

// OpenCV
#include <opencv2/opencv.hpp>

// 自定义头文件
#include "gpio_ctrl.h"

#include "image_utils.h"

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#if defined(RV1106_1103)
#include "dma_alloc.hpp"
#endif

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

// 帧率计算结构
struct fps_counter {
    double fps;
    int frame_count;
    time_t last_time;
};

// 显示器结构体
struct st7789_display {
    int fd;                     // SPI文件描述符
    uint16_t *buffer;           // 显示缓冲区
};

// 创建显示器实例
static struct st7789_display display;
static struct fps_counter g_fps = {0, 0, 0};
static int sockfd = -1;
static volatile bool g_running = true;

// ST7789初始化序列
static const uint16_t st7789_init[] = {
    0x01, 0x80, 150,           // Software reset, 150ms delay
    0x11, 0x80, 255,           // Exit sleep mode, 500ms delay
    0x3A, 0x81, 0x55, 10,      // COLMOD: 16-bit color, 10ms delay
    0x36, 0x01, 0xA0,          // MADCTL: RGB mode
    0x26, 0x01, 0x07,          // 选择伽马曲线 (值范围0-7，越大越暗)
    0xBA, 0x02, 0x08, 0x08,  
    0x21, 0x80, 10,            // Display inversion on, 10ms delay
    0x13, 0x80, 10,            // Normal display mode on, 10ms delay
    0x29, 0x80, 255,           // Display on, 500ms delay
    0x00                       // End of commands
};

// 显示窗口设置命令
static const uint16_t st7789_display_window[] = {
    0x2A, 4, 0, 0, 1, 63,     // Column address set (0-239)
    0x2B, 4, 0, 0, 0, 239,      // Row address set (0-319)
    0x2C,                       // Begin writing to display RAM
    0x00                        // End of commands
};

// 更新帧率的函数
void update_fps() {
    g_fps.frame_count++;
    time_t current_time = time(NULL);
    
    if (current_time != g_fps.last_time) {
        if (g_fps.last_time != 0) {
            g_fps.fps = g_fps.frame_count / (double)(current_time - g_fps.last_time);
        }
        g_fps.frame_count = 0;
        g_fps.last_time = current_time;
    }
}

// SPI传输重试机制
static int spi_transfer_with_retry(struct st7789_display *display, struct spi_ioc_transfer *xfer) {
    for(int i = 0; i < MAX_RETRIES; i++) {
        if(ioctl(display->fd, SPI_IOC_MESSAGE(1), xfer) >= 0) {
            return 0;
        }
        usleep(1000);  // 重试前短暂延迟
    }
    return -1;
}

// 向显示器写入命令或数据
static void write_to_display(struct st7789_display *display, uint8_t value, uint8_t is_data) {
    gpio_set_value(GPIO_DC, is_data);
    
    struct spi_ioc_transfer xfer = {
        .tx_buf = (unsigned long)&value,
        .len = 1,
        .delay_usecs = 0,
        .bits_per_word = 8,
    };
    
    if(spi_transfer_with_retry(display, &xfer) < 0) {
        fprintf(stderr, "SPI transfer failed\n");
        exit(1);
    }
}

// 发送命令序列到显示器
static void send_command_sequence(struct st7789_display *display, const uint16_t *cmds) {
    int i, num_args, ms;
    
    for(i = 0; cmds[i] != 0; ) {
        write_to_display(display, cmds[i++], 0);    // Command
        
        num_args = cmds[i] & 0x7F;         // Number of args
        ms = cmds[i] & 0x80;               // Delay flag
        i++;
        
        while(num_args--) {
            write_to_display(display, cmds[i++], 1); // Data
        }
        
        if(ms) {
            ms = cmds[i++];
            if(ms == 255) ms = 500;
        }
    }
}

// 发送帧缓冲区到显示器
static void send_frame_buffer(struct st7789_display *display, uint16_t *buffer) {
    uint32_t total = BUFFER_SIZE;
    uint32_t sent = 0;
    
    gpio_set_value(GPIO_DC, 1);
    
    while (sent < total) {
        uint32_t chunk_size = ((total - sent) > BLOCK_SIZE) ? BLOCK_SIZE : (total - sent);
        struct spi_ioc_transfer xfer = {
            .tx_buf = (unsigned long)(buffer + (sent / 2)),
            .len = chunk_size,
            .speed_hz = SPI_SPEED, // 降低SPI速度到40MHz，提高稳定性
            .delay_usecs = 0,
            .bits_per_word = 8,
        };
        
        if(spi_transfer_with_retry(display, &xfer) < 0) {
            fprintf(stderr, "Frame buffer transfer failed\n");
            continue; // 不退出，只是跳过这一块
        }
        sent += chunk_size;
    }
}
// RGB888到RGB565转换函数 - 预计算版本
static inline uint16_t rgb888_to_rgb565(uint32_t rgb888) {
    uint8_t r = (rgb888 >> 16) & 0xFF;
    uint8_t g = (rgb888 >> 8) & 0xFF;
    uint8_t b = rgb888 & 0xFF;
    
    // 直接位移转换，避免浮点运算
    uint16_t r5 = (r >> 3) & 0x1F;
    uint16_t g6 = (g >> 2) & 0x3F;
    uint16_t b5 = (b >> 3) & 0x1F;
    
    uint16_t rgb565 = (r5 << 11) | (g6 << 5) | b5;
    return __builtin_bswap16(rgb565);
}

// 灰度世界法自动白平衡
static void auto_white_balance(cv::Mat& image) {
    // 分离BGR通道
    std::vector<cv::Mat> channels;
    cv::split(image, channels);
    
    // 计算每个通道的平均值
    double b_avg = cv::mean(channels[0])[0];
    double g_avg = cv::mean(channels[1])[0];
    double r_avg = cv::mean(channels[2])[0];
    
    // 计算所有通道的平均值
    double avg = (b_avg + g_avg + r_avg) / 3.0;
    
    // 计算增益系数（避免除零）
    double k_b = b_avg > 0 ? avg / b_avg : 1.0;
    double k_g = g_avg > 0 ? avg / g_avg : 1.0;
    double k_r = r_avg > 0 ? avg / r_avg : 1.0;
    
    // 应用增益系数到各通道
    channels[0] = channels[0] * k_b;
    channels[1] = channels[1] * k_g;
    channels[2] = channels[2] * k_r;
    
    // 合并通道
    cv::merge(channels, image);
}

// 优化的图像处理和转换函数，添加白平衡
static void process_and_convert_frame(cv::Mat& frame, uint16_t* display_buffer) {
    static cv::Mat resized(SCREEN_HEIGHT, SCREEN_WIDTH, CV_8UC3);
    
    // 直接缩放到屏幕尺寸
    cv::resize(frame, resized, cv::Size(SCREEN_WIDTH, SCREEN_HEIGHT), 0, 0, cv::INTER_LINEAR);
    
    // 应用自动白平衡
    auto_white_balance(resized);
    
    // 使用指针直接访问像素数据，避免at<>调用开销
    unsigned char* pixels = resized.data;
    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            int idx = (y * SCREEN_WIDTH + x) * 3;
            uint32_t rgb888 = (pixels[idx+2] << 16) | (pixels[idx+1] << 8) | pixels[idx]; // BGR to RGB
            display_buffer[y * SCREEN_WIDTH + x] = rgb888_to_rgb565(rgb888);
        }
    }
}

// 初始化显示器
static void init_display(struct st7789_display *display) {
    // 初始化GPIO
    gpio_init(GPIO_DC);
    gpio_init(GPIO_RES);

    // 打开SPI设备
    display->fd = open(DISPLAY_SPI_DEVICE, O_WRONLY);
    if(display->fd < 0) {
        fprintf(stderr, "无法打开SPI设备\n");
        exit(1);
    }

    // 配置SPI
    uint8_t mode = SPI_MODE_0;
    uint32_t speed = SPI_SPEED;  // 80MHz
    
    if(ioctl(display->fd, SPI_IOC_WR_MODE, &mode) < 0) {
        fprintf(stderr, "无法设置SPI模式\n");
        exit(1);
    }
    if(ioctl(display->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        fprintf(stderr, "无法设置SPI速度\n");
        exit(1);
    }
    
    // 分配帧缓冲区
    display->buffer = (uint16_t *)malloc(BUFFER_SIZE);
    if(!display->buffer) {
        fprintf(stderr, "无法分配帧缓冲区\n");
        exit(1);
    }

    // 硬件复位
    gpio_set_value(GPIO_RES, 1);
    usleep(10000);
    gpio_set_value(GPIO_RES, 0);
    usleep(10000);
    gpio_set_value(GPIO_RES, 1);
    usleep(10000);

    // 初始化显示器
    send_command_sequence(display, st7789_init);
    printf("显示器初始化成功: %s\n", DISPLAY_SPI_DEVICE);
}

int main(int argc, char **argv) {
    // 初始化显示器
    init_display(&display);
    gpio_init(GPIO_BL);
    gpio_set_value(GPIO_BL, 1);
    
    printf("启动摄像头程序...\n");


    // 初始化摄像头
    cv::VideoCapture cap;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
    cap.set(cv::CAP_PROP_FPS, 60);
    cap.open(0);

    if (!cap.isOpened()) {
        printf("无法打开摄像头\n");
        return -1;
    }
    
    // 初始化显示缓冲区为黑色
    memset(display.buffer, 0, BUFFER_SIZE);
    
    // 设置显示窗口 (仅需设置一次)
    send_command_sequence(&display, st7789_display_window);
    
    // 发送一个黑屏帧
    send_frame_buffer(&display, display.buffer);
    
    // 等待显示器稳定
    usleep(100000); // 100ms

    printf("开始主循环处理...\n");
    
    cv::Mat frame;
    

    while (g_running) {
        // 获取一帧图像
        cap >> frame;
        if (frame.empty()) {
            printf("读取摄像头帧失败\n");
            continue;
        }

        // 更新帧率
        update_fps();

        // 处理图像并转换为显示格式
        process_and_convert_frame(frame, display.buffer);
    
        // 显示图像 (无需每帧都重新发送窗口设置命令)
        // 仅开始写入RAM和发送帧缓冲区
        write_to_display(&display, 0x2C, 0); // 开始写入RAM命令
        send_frame_buffer(&display, display.buffer);
    
        
        // 打印当前帧率
        printf("\r当前帧率: %.2f FPS", g_fps.fps);
        fflush(stdout);
    }

    printf("\n程序退出中...\n");
    
    // 清理资源
    if (sockfd >= 0) {
        close(sockfd);
    }
    
    cap.release();
    
    if (display.buffer) {
        free(display.buffer);
    }
    
    close(display.fd);
    gpio_cleanup(GPIO_DC);
    gpio_cleanup(GPIO_RES);
    gpio_cleanup(GPIO_BL);
    
    return 0;
}