/**
 * RGAOperations.h
 *
 * 摘要：
 * 该头文件声明了RGAOperations类，用于封装Rockchip GPU加速器(RGA)相关的图像处理操作。
 * RGA是瑞芯微(Rockchip)芯片上的2D图形加速硬件，用于高效执行图像缩放、混合、旋转等操作。
 * 此类提供了图像缩放、图像混合和矩形绘制等功能的硬件加速实现，适用于需要高性能图像处理的场景。
 */

#ifndef RGA_RESIZER_H
#define RGA_RESIZER_H

// 包含必要的标准库和第三方库头文件
#include <cstdint>    // 提供固定大小的整数类型定义
#include <mutex>      // 提供互斥锁功能，用于线程同步
#include <rga/im2d.h> // RGA的图像处理接口，提供2D操作函数
#include <rga/rga.h>  // RGA的基础功能接口
#include <rga/im2d.h> // 重复包含，可能是疏忽，建议移除
#include <stdexcept>  // 提供标准异常类
#include <cstring>    // 提供内存操作函数

#include <iostream>           // 标准输入输出流
#include <opencv2/opencv.hpp> // OpenCV库，用于图像处理
#include <Utils.h>            // 项目自定义工具类

/**
 * RGAOperations类：提供使用RGA硬件加速的图像处理操作
 */
class RGAOperations
{
private:
    // 静态互斥锁，用于保护RGA资源在多线程环境下的访问安全
    static std::mutex rga_mutex;

public:
    /**
     * 使用RGA硬件加速调整图像大小
     *
     * @param src_data 源图像数据指针
     * @param src_width 源图像宽度
     * @param src_height 源图像高度
     * @param src_format 源图像格式，使用RGA定义的格式常量
     * @param dst_width 目标图像宽度
     * @param dst_height 目标图像高度
     * @param dst_format 目标图像格式，使用RGA定义的格式常量
     * @param dst_size 输入/输出参数，期望的目标图像大小（字节数）
     * @return 返回调整大小后的图像数据指针，调用者负责释放内存
     */
    // static unsigned char *resizeImage(const unsigned char *src_data,
    //                                   int src_width, int src_height, int src_format,
    //                                   int dst_width, int dst_height, int dst_format,
    //                                   int &dst_size);
    static unsigned char *resizeImage(const unsigned char *src_data,
                                      int src_width, int src_height, int src_format,
                                      int dst_width, int dst_height, int dst_format,
                                      int &dst_size, const char *caller_id);

    // 注释掉的旧版本函数声明
    // void blendImages(cv::Mat &dst_image, const cv::Mat &src_image);

    /**
     * 使用RGA硬件加速混合两张图像
     *
     * @param dst_image 目标图像（OpenCV格式），也作为结果输出
     * @param src_image 源图像（OpenCV格式）
     */
    static void blendImages(cv::Mat &dst_image, const cv::Mat &src_image);

    /**
     * 使用RGA硬件加速在图像上绘制矩形
     *
     * @param dst_data 目标图像数据指针
     * @param width 图像宽度
     * @param height 图像高度
     * @param bbox 要绘制的矩形区域（OpenCV格式）
     * @return RGA操作状态码
     */
    IM_STATUS convert_and_draw_rectangle(uint8_t *dst_data, int width, int height, const cv::Rect &bbox);
};

#endif // RGA_RESIZER_H