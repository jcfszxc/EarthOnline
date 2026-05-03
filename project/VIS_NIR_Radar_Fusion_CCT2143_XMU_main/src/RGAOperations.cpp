/**
 * RGAOperations.cpp
 *
 * 摘要：
 * 本文件实现了RGA（Rockchip GPU Accelerator）相关操作的功能类，
 * 提供了图像缩放、图像混合和矩形绘制等GPU加速功能。
 * 使用互斥锁确保RGA资源的线程安全访问。
 */

#include "RGAOperations.h"

// 定义RGA操作的互斥锁，确保多线程环境下RGA操作的线程安全
std::mutex RGAOperations::rga_mutex;

/**
 * 使用RGA硬件加速调整图像大小
 *
 * @param src_data 源图像数据指针
 * @param src_width 源图像宽度
 * @param src_height 源图像高度
 * @param src_format 源图像格式
 * @param dst_width 目标图像宽度
 * @param dst_height 目标图像高度
 * @param dst_format 目标图像格式
 * @param dst_size 输出参数，目标图像大小（字节数）
 * @return 返回调整大小后的图像数据指针
 */

 unsigned char *RGAOperations::resizeImage(const unsigned char *src_data,
                                         int src_width, int src_height, int src_format,
                                         int dst_width, int dst_height, int dst_format,
                                         int &dst_size, const char *caller_id)
{
    // 使用互斥锁确保线程安全
    std::lock_guard<std::mutex> lock(rga_mutex);
    
    // 如果提供了调用者ID，则打印它
    if (caller_id)
    {
        std::cout << "RGAOperation called by: " << caller_id << std::endl;
    }

    // 检查源数据是否为空
    if (!src_data)
    {
        throw std::runtime_error("Source data is null");
    }

    // 为目标图像分配内存
    unsigned char *dst_data = new unsigned char[dst_size];

    // 创建源图像的RGA缓冲区
    rga_buffer_t src_buf = wrapbuffer_virtualaddr(
        (void *)src_data,
        src_width,
        src_height,
        src_format);

    // 创建目标图像的RGA缓冲区
    rga_buffer_t dst_buf = wrapbuffer_virtualaddr(
        (void *)dst_data,
        dst_width,
        dst_height,
        dst_format);

    // 定义源图像和目标图像的矩形区域
    im_rect src_rect = {0, 0, src_width, src_height};
    im_rect dst_rect = {0, 0, dst_width, dst_height};

    // 检查RGA操作参数是否有效
    int ret = imcheck(src_buf, dst_buf, src_rect, dst_rect);
    if (ret != IM_STATUS_NOERROR)
    {
        // 参数无效，释放内存并抛出异常
        delete[] dst_data;
        throw std::runtime_error("RGA check error: " + std::string(imStrError((IM_STATUS)ret)));
    }

    // 执行RGA图像缩放操作
    {
        ret = imresize(src_buf, dst_buf);
    }

    // 检查RGA操作结果，处理错误情况
    if (ret != IM_STATUS_SUCCESS && ret != IM_STATUS_NOERROR && ret != 1)
    {
        delete[] dst_data;
        std::cerr << "RGA resize error. Return value: " << ret << std::endl;
        std::cerr << "Error description: " << imStrError((IM_STATUS)ret) << std::endl;
        throw std::runtime_error("RGA resize failed. Check error output for details.");
    }
    else
    {
        // 操作成功
    }

    // 返回处理后的图像数据
    return dst_data;
}

// unsigned char *RGAOperations::resizeImage(const unsigned char *src_data,
//                                           int src_width, int src_height, int src_format,
//                                           int dst_width, int dst_height, int dst_format,
//                                           int &dst_size)
// {
//     // 使用互斥锁确保线程安全
//     std::lock_guard<std::mutex> lock(rga_mutex);
//     // // 开始计时的代码（已注释）
//     // auto start = std::chrono::high_resolution_clock::now();

//     // 检查源数据是否为空
//     if (!src_data)
//     {
//         throw std::runtime_error("Source data is null");
//     }

//     // 为目标图像分配内存
//     unsigned char *dst_data = new unsigned char[dst_size];

//     // 创建源图像的RGA缓冲区
//     rga_buffer_t src_buf = wrapbuffer_virtualaddr(
//         (void *)src_data,
//         src_width,
//         src_height,
//         src_format);

//     // 创建目标图像的RGA缓冲区
//     rga_buffer_t dst_buf = wrapbuffer_virtualaddr(
//         (void *)dst_data,
//         dst_width,
//         dst_height,
//         dst_format);

//     // 定义源图像和目标图像的矩形区域
//     im_rect src_rect = {0, 0, src_width, src_height};
//     im_rect dst_rect = {0, 0, dst_width, dst_height};

//     // 检查RGA操作参数是否有效
//     int ret = imcheck(src_buf, dst_buf, src_rect, dst_rect);
//     if (ret != IM_STATUS_NOERROR)
//     {
//         // 参数无效，释放内存并抛出异常
//         delete[] dst_data;
//         throw std::runtime_error("RGA check error: " + std::string(imStrError((IM_STATUS)ret)));
//     }

//     // 执行RGA图像缩放操作
//     {
//         ret = imresize(src_buf, dst_buf);
//     }

//     // 检查RGA操作结果，处理错误情况
//     if (ret != IM_STATUS_SUCCESS && ret != IM_STATUS_NOERROR && ret != 1)
//     {
//         delete[] dst_data;
//         std::cerr << "RGA resize error. Return value: " << ret << std::endl;
//         std::cerr << "Error description: " << imStrError((IM_STATUS)ret) << std::endl;
//         throw std::runtime_error("RGA resize failed. Check error output for details.");
//     }
//     else
//     {
//         // 操作成功（日志输出已注释）
//         // std::cout << "RGA resize operation completed. Return value: " << ret << std::endl;
//     }

//     // // 计时结束的代码（已注释）
//     // auto end = std::chrono::high_resolution_clock::now();
//     // // 计算持续时间（以毫秒为单位）
//     // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//     // // 打印持续时间
//     // std::cout << "resizeImage 执行时间: " << duration.count() << " 毫秒" << std::endl;

//     // 返回处理后的图像数据
//     return dst_data;
// }


// unsigned char *RGAOperations::resizeImage(const unsigned char *src_data,
//                                           int src_width, int src_height, int src_format,
//                                           int dst_width, int dst_height, int dst_format,
//                                           int &dst_size)
// {
//     std::lock_guard<std::mutex> lock(rga_mutex);

//     if (!src_data)
//     {
//         throw std::runtime_error("Source data is null");
//     }

//     std::cout << "resizeImage!" << std::endl;

//     // 强制创建对齐错误 - 方法1：修改宽度使其不是16的倍数
//     // 如果宽度是16的倍数，则减1使其不对齐
//     if (src_width % 16 == 0) {
//         std::cerr << "警告：强制创建宽度对齐错误，原宽度:" << src_width << " 修改为:" << (src_width - 1) << std::endl;
//         src_width = src_width - 1;
//     }

//     // 强制创建对齐错误 - 方法2：修改高度使其不是16的倍数
//     // 如果高度是16的倍数，则减1使其不对齐
//     if (src_height % 16 == 0) {
//         std::cerr << "警告：强制创建高度对齐错误，原高度:" << src_height << " 修改为:" << (src_height - 1) << std::endl;
//         src_height = src_height - 1;
//     }

//     // 或者可以选择修改目标尺寸
//     // if (dst_width % 16 == 0) dst_width -= 1;
//     // if (dst_height % 16 == 0) dst_height -= 1;

//     unsigned char *dst_data = new unsigned char[dst_size];

//     // 强制创建对齐错误 - 方法3：创建一个内存对齐错误（地址不是16字节对齐）
//     // 创建一个新的缓冲区，偏移1字节
//     // 注释下面三行代码可以禁用此方法
//     /*
//     unsigned char* unaligned_buffer = new unsigned char[dst_size + 1];
//     delete[] dst_data; // 释放原来分配的内存
//     dst_data = unaligned_buffer + 1; // 使用非对齐地址
//     std::cerr << "警告：强制创建内存地址对齐错误，地址偏移1字节" << std::endl;
//     */

//     rga_buffer_t src_buf = wrapbuffer_virtualaddr(
//         (void *)src_data,
//         src_width,
//         src_height,
//         src_format);

//     rga_buffer_t dst_buf = wrapbuffer_virtualaddr(
//         (void *)dst_data,
//         dst_width,
//         dst_height,
//         dst_format);

//     im_rect src_rect = {0, 0, src_width, src_height};
//     im_rect dst_rect = {0, 0, dst_width, dst_height};

//     // 打印对齐信息用于调试
//     std::cerr << "源尺寸: " << src_width << "x" << src_height 
//               << " (宽度mod16=" << (src_width % 16) 
//               << ", 高度mod16=" << (src_height % 16) << ")" << std::endl;
//     std::cerr << "目标尺寸: " << dst_width << "x" << dst_height 
//               << " (宽度mod16=" << (dst_width % 16) 
//               << ", 高度mod16=" << (dst_height % 16) << ")" << std::endl;
    
//     // 打印内存地址用于调试对齐问题
//     uintptr_t src_addr = reinterpret_cast<uintptr_t>(src_data);
//     uintptr_t dst_addr = reinterpret_cast<uintptr_t>(dst_data);
//     std::cerr << "源地址: " << src_addr << " (mod16=" << (src_addr % 16) << ")" << std::endl;
//     std::cerr << "目标地址: " << dst_addr << " (mod16=" << (dst_addr % 16) << ")" << std::endl;

//     int ret = imcheck(src_buf, dst_buf, src_rect, dst_rect);
//     if (ret != IM_STATUS_NOERROR)
//     {
//         delete[] dst_data;
//         throw std::runtime_error("RGA check error: " + std::string(imStrError((IM_STATUS)ret)));
//     }

//     {
//         ret = imresize(src_buf, dst_buf);
//     }

//     if (ret != IM_STATUS_SUCCESS && ret != IM_STATUS_NOERROR && ret != 1)
//     {
//         delete[] dst_data;
//         std::cerr << "RGA resize error. Return value: " << ret << std::endl;
//         std::cerr << "Error description: " << imStrError((IM_STATUS)ret) << std::endl;
//         throw std::runtime_error("RGA resize failed. Check error output for details.");
//     }
//     else
//     {
//         std::cerr << "RGA操作完成，返回值: " << ret << std::endl;
//     }

//     // 如果使用了方法3（内存偏移），需要修改返回方式以避免内存泄漏
//     // return unaligned_buffer; // 注意：如果使用了方法3，需取消此行注释并注释下面的return语句

//     return dst_data;
// }

/**
 * 使用RGA硬件加速混合两张图像
 *
 * @param dst_image 目标图像（OpenCV格式），也作为结果输出
 * @param src_image 源图像（OpenCV格式）
 */
void RGAOperations::blendImages(cv::Mat &dst_image, const cv::Mat &src_image)
{
    // 检查源图像和目标图像是否为空
    if (src_image.empty() || dst_image.empty())
    {
        throw std::runtime_error("Source or destination image is empty");
    }

    // 为源图像和目标图像添加半透明的Alpha通道
    cv::Mat src_rgba = Utils::set_alpha_channel(src_image, 127); // 半透明
    cv::Mat dst_rgba = Utils::set_alpha_channel(dst_image, 127); // 半透明

    // 将OpenCV图像转换为RGA缓冲区
    rga_buffer_t src_buf = Utils::cv_mat_to_rga_buffer(src_rgba);
    rga_buffer_t dst_buf = Utils::cv_mat_to_rga_buffer(dst_rgba);

    // 执行RGA图像混合操作
    IM_STATUS ret;
    {
        // 使用互斥锁确保线程安全
        std::lock_guard<std::mutex> lock(rga_mutex);
        // 执行混合操作，使用源图像叠加模式和预乘Alpha
        ret = imblend(src_buf, dst_buf, IM_ALPHA_BLEND_SRC_OVER | IM_ALPHA_BLEND_PRE_MUL);
    }

    // 检查混合操作结果
    if (ret != IM_STATUS_SUCCESS)
    {
        throw std::runtime_error("Error blending images: " + std::string(imStrError(ret)));
    }

    // 将混合结果更新到目标图像
    // dst_image = cv::Mat(dst_buf.height, dst_buf.width, CV_8UC4, dst_buf.vir_addr);
    cv::Mat temp(dst_buf.height, dst_buf.width, CV_8UC4, dst_buf.vir_addr);
    temp.copyTo(dst_image);
}

/**
 * 使用RGA硬件加速在图像上绘制矩形
 *
 * @param dst_data 目标图像数据指针
 * @param width 图像宽度
 * @param height 图像高度
 * @param bbox 要绘制的矩形区域
 * @return RGA操作状态
 */
IM_STATUS RGAOperations::convert_and_draw_rectangle(uint8_t *dst_data, int width, int height, const cv::Rect &bbox)
{
    // 创建RGA缓冲区结构并初始化为0
    rga_buffer_t dst;
    memset(&dst, 0, sizeof(rga_buffer_t));

    // 设置RGA缓冲区的属性
    dst.width = width;
    dst.height = height;
    dst.wstride = width;
    dst.hstride = height;
    dst.format = RK_FORMAT_BGR_888; // 假设输入格式是BGR

    // 使用虚拟地址包装RGA缓冲区
    dst = wrapbuffer_virtualaddr(dst_data, width, height, RK_FORMAT_BGR_888);

    // 将OpenCV的Rect转换为RGA的im_rect结构
    im_rect rga_rect = {
        .x = static_cast<int>(bbox.x),
        .y = static_cast<int>(bbox.y),
        .width = static_cast<int>(bbox.width),
        .height = static_cast<int>(bbox.height)};

    // 设置矩形颜色为绿色（ABGR格式）
    uint32_t color = 0x00FF0000; // ABGR格式

    // 确保矩形尺寸至少为2x2（RGA要求）
    if (rga_rect.width < 2)
        rga_rect.width = 2;
    if (rga_rect.height < 2)
        rga_rect.height = 2;

    // 使用RGA绘制矩形，线宽为2像素
    IM_STATUS status = imrectangle(dst, rga_rect, color, 2);

    // 返回操作状态
    return status;
}