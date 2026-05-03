/**
 * @file Utils.cpp
 * @brief 工具函数库实现文件
 *
 * 本文件包含了一系列实用工具函数，主要功能包括：
 * - 校验和计算
 * - 系统运行时间获取
 * - 字节数据转16进制字符串
 * - 线程绑定到特定CPU核心
 * - 图像处理（红外、深度图映射到彩色空间）
 * - RGA缓冲区操作
 * - 图像矩形绘制
 */

#include "Utils.h"
#include <sys/sysinfo.h> // 提供系统信息相关函数
#include <algorithm>     // 提供算法函数如min、max等

namespace Utils
{
    /**
     * @brief 计算数据缓冲区的校验和
     * @param buffer 要计算校验和的数据缓冲区
     * @param size 数据缓冲区的大小
     * @return 计算得到的8位校验和
     */
    uint8_t calculateChecksum(const uint8_t *buffer, size_t size)
    {
        uint8_t checksum = 0; // 初始化校验和为0
        for (size_t i = 0; i < size; ++i)
        {
            checksum += buffer[i]; // 累加每个字节的值
        }
        return checksum; // 返回计算得到的校验和
    }

    /**
     * @brief 获取系统运行时间（以百分之一小时为单位）
     * @return 系统运行时间，范围为0-65535
     */
    uint16_t getSystemUptime()
    {
        struct sysinfo sys_info; // 定义系统信息结构体
        if (sysinfo(&sys_info) != 0)
        {
            return 0; // 如果获取系统信息失败，返回0（或者可以抛出异常）
        }

        long uptime_seconds = sys_info.uptime;                                            // 获取系统运行时间（秒）
        uint16_t uptime_hundredths_of_hours = static_cast<uint16_t>(uptime_seconds / 36); // 转换为百分之一小时
        return std::min(uptime_hundredths_of_hours, static_cast<uint16_t>(65535));        // 确保不超过16位无符号整数的最大值
    }

    /**
     * @brief 将字节数组转换为16进制字符串
     * @param data 字节数组指针
     * @param length 数组长度
     * @return 格式化后的16进制字符串，如 "0x01 0x02 0x03"
     */
    std::string bytesToHexString(const uint8_t *data, int length)
    {
        std::stringstream ss; // 创建字符串流
        for (int i = 0; i < length; ++i)
        {
            if (i > 0)
            {
                ss << " "; // 在每个值之间添加空格
            }
            ss << "0x" << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<int>(data[i]); // 格式化为16进制，确保两位宽度并以0填充
        }
        return ss.str(); // 返回字符串
    }

    /**
     * @brief 将当前线程绑定到指定的CPU核心
     * @param cpu_id 目标CPU核心ID
     */
    void bind_to_cpu(int cpu_id)
    {
        cpu_set_t cpu_set;                         // 定义CPU集合
        CPU_ZERO(&cpu_set);                        // 清空CPU集合
        CPU_SET(cpu_id, &cpu_set);                 // 设置指定的CPU核心
        pthread_t current_thread = pthread_self(); // 获取当前线程ID
        if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpu_set) != 0)
        {
            std::cerr << "Failed to bind thread to CPU " << cpu_id << std::endl; // 绑定失败时输出错误信息
        }
        else
        {
            std::cout << "Successfully bound thread to CPU " << cpu_id << std::endl; // 绑定成功时输出信息
        }
    }

    /**
     * @brief 将当前线程绑定到多个CPU核心（当前已被注释禁用）
     * @param cpu_ids 目标CPU核心ID列表
     */
    void bind_to_cpus(const std::vector<int> &cpu_ids)
    {
        // 注：此函数实现已被注释禁用
        // cpu_set_t cpu_set;
        // CPU_ZERO(&cpu_set);

        // for (int cpu_id : cpu_ids)
        // {
        //     CPU_SET(cpu_id, &cpu_set);
        // }

        // pthread_t current_thread = pthread_self();
        // if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpu_set) != 0)
        // {
        //     std::cerr << "Failed to bind thread to CPUs";
        //     for (int cpu_id : cpu_ids)
        //     {
        //         std::cerr << " " << cpu_id;
        //     }
        //     std::cerr << std::endl;
        // }
        // else
        // {
        //     std::cout << "Successfully bound thread to CPUs";
        //     for (int cpu_id : cpu_ids)
        //     {
        //         std::cout << " " << cpu_id;
        //     }
        //     std::cout << std::endl;
        // }
    }

    /**
     * @brief 将红外图像值映射到绿色空间（绿色渐变效果）
     * @param irValue 红外图像像素值
     * @return 映射后的RGB颜色
     */
    cv::Vec3b mapIRToGreenSpace(cv::Vec3b irValue)
    {
        cv::Vec3b mappedColor;                          // 定义返回的颜色向量
        uint8_t threshold = 128;                        // 定义阈值
        uint8_t isLow = irValue[0] < threshold ? 1 : 0; // 判断是否低于阈值

        // 根据是否低于阈值应用不同的颜色映射
        mappedColor[0] = static_cast<uint8_t>((irValue[0] - threshold) * 2 * (1 - isLow));           // B通道
        mappedColor[1] = static_cast<uint8_t>((irValue[0] + threshold) * isLow + 255 * (1 - isLow)); // G通道（主要用于显示）
        mappedColor[2] = 0;                                                                          // R通道

        return mappedColor; // 返回映射后的颜色
    }

    /**
     * @brief 将红外图像值映射到伪彩色（使用JET色彩映射）
     * @param irValue 红外图像像素值
     * @return 映射后的RGB颜色
     */
    cv::Vec3b mapIRToPseudoColor(cv::Vec3b irValue)
    {
        cv::Mat singleChannelMat(1, 1, CV_8UC1, cv::Scalar(irValue[0]));       // 创建单通道矩阵
        cv::Mat pseudoColorMat;                                                // 定义伪彩色矩阵
        cv::applyColorMap(singleChannelMat, pseudoColorMat, cv::COLORMAP_JET); // 应用JET颜色映射
        return pseudoColorMat.at<cv::Vec3b>(0, 0);                             // 返回映射后的颜色
    }

    /**
     * @brief 将距离值映射到彩色空间
     * @param distanceValue 距离值（0-255）
     * @return 映射后的RGB颜色
     */
    cv::Vec3b mapDistanceToColor(uint8_t distanceValue)
    {
        cv::Vec3b mappedColor;                             // 定义返回的颜色向量
        uint8_t threshold = 128;                           // 定义阈值
        uint8_t isLow = distanceValue < threshold ? 1 : 0; // 判断是否低于阈值

        // 根据距离值计算对应的颜色
        mappedColor[0] = static_cast<uint8_t>((distanceValue - threshold) * 2 * (1 - isLow));           // B通道
        mappedColor[1] = static_cast<uint8_t>((distanceValue + threshold) * isLow + 255 * (1 - isLow)); // G通道
        mappedColor[2] = static_cast<uint8_t>(255 - distanceValue);                                     // R通道

        return mappedColor; // 返回映射后的颜色
    }

    /**
     * @brief 将OpenCV的Mat转换为RGA缓冲区（用于RockChip GPU加速）
     * @param mat 输入的OpenCV Mat
     * @return 转换后的RGA缓冲区
     */
    rga_buffer_t cv_mat_to_rga_buffer(const cv::Mat &mat)
    {
        rga_buffer_t rga_buf;                      // 定义RGA缓冲区
        memset(&rga_buf, 0, sizeof(rga_buffer_t)); // 初始化缓冲区为0

        rga_buf.width = mat.cols;                                                       // 设置宽度
        rga_buf.height = mat.rows;                                                      // 设置高度
        rga_buf.wstride = mat.step / mat.elemSize();                                    // 设置行跨度
        rga_buf.hstride = mat.rows;                                                     // 设置列跨度
        rga_buf.format = mat.channels() == 4 ? RK_FORMAT_BGRA_8888 : RK_FORMAT_BGR_888; // 根据通道数设置格式
        rga_buf.vir_addr = mat.data;                                                    // 设置虚拟地址

        return rga_buf; // 返回RGA缓冲区
    }

    /**
     * @brief 设置输入图像的Alpha通道
     * @param input 输入的BGRA图像
     * @param alpha_value Alpha值（0-255，默认127）
     * @return 设置了Alpha通道的图像
     */
    cv::Mat set_alpha_channel(const cv::Mat &input, uchar alpha_value)
    {
        CV_Assert(input.channels() == 4); // 确保输入是BGRA格式

        cv::Mat result = input.clone(); // 克隆输入图像

        // 创建指定Alpha值的单通道矩阵
        cv::Mat alpha(input.size(), CV_8UC1, cv::Scalar(alpha_value));

        int from_to[] = {0, 3};                             // 从通道0（alpha矩阵中唯一的通道）到通道3（result中的alpha通道）
        cv::mixChannels(&alpha, 1, &result, 1, from_to, 1); // 混合通道

        return result; // 返回设置了Alpha通道的图像
    }

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
    void draw_rectangle(uint8_t *data, int width, int height, int x, int y,
                        int rect_width, int rect_height,
                        uint8_t y_value, uint8_t u_value, uint8_t v_value)
    {
        // 输出调试信息
        std::cout << "Debug - Image size: " << width << "x" << height << std::endl;
        std::cout << "Debug - Drawing at: (" << x << "," << y << ") size: "
                  << rect_width << "x" << rect_height << std::endl;

        // 基本参数检查
        if (!data)
        {
            std::cerr << "Error: Null data pointer" << std::endl; // 空指针错误
            return;
        }

        if (width <= 0 || height <= 0)
        {
            std::cerr << "Error: Invalid dimensions" << std::endl; // 无效尺寸错误
            return;
        }

        // 边界检查和裁剪
        x = std::max(0, x); // 确保x不小于0
        y = std::max(0, y); // 确保y不小于0

        // 确保矩形不会超出图像边界
        rect_width = std::min(rect_width, width - x);
        rect_height = std::min(rect_height, height - y);

        if (rect_width <= 0 || rect_height <= 0)
        {
            std::cerr << "Error: Invalid rectangle size after clipping" << std::endl; // 裁剪后矩形无效
            return;
        }

        // 计算YUV420格式的平面大小
        size_t y_plane_size = width * height;                   // Y平面大小
        size_t uv_plane_size = (width * height) / 4;            // UV平面各占Y平面的1/4
        size_t total_size = y_plane_size + (uv_plane_size * 2); // 总大小

        // Y平面绘制
        try
        {
            // 绘制水平线（上边和下边）
            for (int i = x; i < x + rect_width; ++i)
            {
                // if (y * width + i < y_plane_size)
                if (static_cast<size_t>(y * width + i) < y_plane_size)
                {
                    data[y * width + i] = y_value; // 上边
                }
                // if ((y + rect_height - 1) * width + i < y_plane_size)
                if (static_cast<size_t>((y + rect_height - 1) * width + i) < y_plane_size)
                {
                    data[(y + rect_height - 1) * width + i] = y_value; // 下边
                }
            }

            // 绘制垂直线（左边和右边）
            for (int i = y; i < y + rect_height; ++i)
            {
                // if (i * width + x < y_plane_size)
                if (static_cast<size_t>(i * width + x) < y_plane_size)
                {
                    data[i * width + x] = y_value; // 左边
                }
                // if (i * width + (x + rect_width - 1) < y_plane_size)
                if (static_cast<size_t>(i * width + (x + rect_width - 1)) < y_plane_size)
                {
                    data[i * width + (x + rect_width - 1)] = y_value; // 右边
                }
            }

            // UV平面处理
            int uv_width = width / 2;                   // UV平面宽度为Y平面的一半
            int uv_x = x / 2;                           // UV平面中的x坐标
            int uv_y = y / 2;                           // UV平面中的y坐标
            int uv_rect_width = (rect_width + 1) / 2;   // UV平面中矩形宽度
            int uv_rect_height = (rect_height + 1) / 2; // UV平面中矩形高度
            size_t uv_offset = y_plane_size;            // UV平面起始偏移

            // 在UV平面上绘制边界
            for (int i = uv_y; i < uv_y + uv_rect_height && i < height / 2; ++i)
            {
                for (int j = uv_x; j < uv_x + uv_rect_width && j < uv_width; ++j)
                {
                    size_t uv_index = uv_offset + (i * uv_width + j) * 2;
                    if (uv_index + 1 < total_size)
                    {
                        data[uv_index] = u_value;     // U通道
                        data[uv_index + 1] = v_value; // V通道
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error during drawing: " << e.what() << std::endl; // 捕获并输出绘制过程中的异常
        }
    }
} // namespace Utils