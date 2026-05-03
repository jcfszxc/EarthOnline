/**
 * @file Utils.h
 * @brief 工具函数库头文件
 *
 * 本文件定义了一系列实用工具函数的声明，包括：
 * - 数据处理功能：校验和计算、系统运行时间获取、字节数组转16进制字符串
 * - 线程优化功能：线程绑定到特定CPU核心
 * - 图像处理功能：红外图像颜色映射、深度图颜色映射、RGA缓冲区操作、图像绘制等
 */

#ifndef UTILS_H // 防止头文件重复包含的宏定义开始
#define UTILS_H

#include <cstdint>            // 提供标准整数类型定义
#include <cstddef>            // 提供size_t等类型定义
#include <iostream>           // 提供标准输入输出流
#include <pthread.h>          // 提供POSIX线程相关功能
#include <sched.h>            // 提供调度相关功能
#include <sstream>            // 提供字符串流功能
#include <iomanip>            // 提供格式化输出功能
#include <opencv2/opencv.hpp> // 提供OpenCV图像处理功能
#include <rga/im2d.h>         // 提供RockChip RGA图像处理功能
#include <rga/rga.h>          // 提供RockChip RGA功能

#include <arpa/inet.h>  // 提供网络地址相关功能：sockaddr_in, AF_INET, htons, inet_addr
#include <sys/socket.h> // 提供套接字相关功能：sendto等
#include <thread>       // 提供C++线程支持：std::this_thread

// 如果使用GNU扩展，可能需要定义以下宏：
#ifndef _GNU_SOURCE
#define _GNU_SOURCE // 启用GNU特定扩展
#endif

/**
 * @brief 混合模式枚举类
 *
 * 定义不同的图像混合模式：
 * - V: 可见光图像
 * - I: 红外图像
 * - VL: 可见光图像与轮廓
 * - IL: 红外图像与轮廓
 * - VI: 可见光与红外图像
 * - VIL: 可见光、红外图像与轮廓
 * - L: 仅轮廓
 */
enum class BlendType
{
    V,   // 可见光图像
    I,   // 红外图像
    VL,  // 可见光图像与轮廓
    IL,  // 红外图像与轮廓
    VI,  // 可见光与红外图像
    VIL, // 可见光、红外图像与轮廓
    L    // 仅轮廓
};

namespace Utils
{
    /**
     * @brief 计算数据缓冲区的校验和
     * @param buffer 要计算校验和的数据缓冲区
     * @param size 数据缓冲区的大小
     * @return 计算得到的8位校验和
     */
    uint8_t calculateChecksum(const uint8_t *buffer, size_t size);

    /**
     * @brief 获取系统运行时间（以百分之一小时为单位）
     * @return 系统运行时间，范围为0-65535
     */
    uint16_t getSystemUptime();

    /**
     * @brief 将当前线程绑定到指定的CPU核心
     * @param cpu_id 目标CPU核心ID
     */
    void bind_to_cpu(int cpu_id);

    /**
     * @brief 将当前线程绑定到多个CPU核心
     * @param cpu_ids 目标CPU核心ID列表
     */
    void bind_to_cpus(const std::vector<int> &cpu_ids);

    /**
     * @brief 将字节数组转换为16进制字符串
     * @param data 字节数组指针
     * @param length 数组长度
     * @return 格式化后的16进制字符串，如 "0x01 0x02 0x03"
     */
    std::string bytesToHexString(const uint8_t *data, int length);

    /**
     * @brief 将红外图像值映射到绿色空间（绿色渐变效果）
     * @param irValue 红外图像像素值
     * @return 映射后的RGB颜色
     */
    cv::Vec3b mapIRToGreenSpace(cv::Vec3b irValue);

    /**
     * @brief 将红外图像值映射到伪彩色（使用JET色彩映射）
     * @param irValue 红外图像像素值
     * @return 映射后的RGB颜色
     */
    cv::Vec3b mapIRToPseudoColor(cv::Vec3b irValue);

    /**
     * @brief 将距离值映射到彩色空间
     * @param distanceValue 距离值（0-255）
     * @return 映射后的RGB颜色
     */
    cv::Vec3b mapDistanceToColor(uint8_t distanceValue);

    /**
     * @brief 将OpenCV的Mat转换为RGA缓冲区（用于RockChip GPU加速）
     * @param mat 输入的OpenCV Mat
     * @return 转换后的RGA缓冲区
     */
    rga_buffer_t cv_mat_to_rga_buffer(const cv::Mat &mat);

    /**
     * @brief 设置输入图像的Alpha通道
     * @param input 输入的BGRA图像
     * @param alpha_value Alpha值（0-255，默认127）
     * @return 设置了Alpha通道的图像
     */
    cv::Mat set_alpha_channel(const cv::Mat &input, uchar alpha_value);

    /**
     * @brief 在YUV420图像上绘制矩形
     * @param data 图像数据指针
     * @param width 图像宽度
     * @param height 图像高度
     * @param x 矩形左上角x坐标
     * @param y 矩形左上角y坐标
     * @param rect_width 矩形宽度
     * @param rect_height 矩形高度
     * @param y_value Y通道值
     * @param u_value U通道值
     * @param v_value V通道值
     */
    void draw_rectangle(uint8_t *data, int width, int height, int x, int y, int rect_width, int rect_height, uint8_t y_value, uint8_t u_value, uint8_t v_value);

} // namespace Utils

#endif // UTILS_H  // 防止头文件重复包含的宏定义结束