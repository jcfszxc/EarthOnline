/**
 * WorkFlow.cpp - 船舶多传感器融合处理系统的工作流实现
 *
 * 本文件实现了船舶监控系统中的核心工作流程，集成多种传感器数据并进行实时处理。
 * 主要功能包括：
 * 1. 视频处理 - 红外与可见光图像采集、处理、编码和显示
 * 2. 网络通信 - 接收控制指令、发送状态帧和视频流数据
 * 3. 数据融合 - 多源数据（可见光、红外、激光雷达）的配准与融合
 * 4. 目标检测 - 基于神经网络的实时目标识别与跟踪
 * 5. 激光测距 - 点云数据处理、距离计算和可视化显示
 * 6. 多线程管理 - 高效的并行处理确保实时性能
 * 7. 模式切换 - 支持多种显示模式（单画面/多画面、融合/非融合等）
 * 8. 资源管理 - GStreamer视频管道、OpenCV图像处理和RGA硬件加速
 *
 * 该模块是船舶监控系统的核心处理单元，支持多种工作模式，适用于昼夜全天候监控场景。
 */

#include "WorkFlow.h"
#include "NetworkManager.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <random>

// 添加接收box数据的函数
// 通过UDP socket接收矩形框坐标信息
bool WorkFlow::receiveBoxCoordinates(int socket_fd, struct sockaddr_in &sender_addr)
{
    BOX_RECT received_box;                           // 定义接收的矩形框结构
    socklen_t sender_addr_len = sizeof(sender_addr); // 发送方地址长度

    // 接收数据 - 使用recvfrom函数接收UDP数据
    ssize_t received_bytes = recvfrom(socket_fd,
                                      &received_box,
                                      sizeof(BOX_RECT),
                                      0,
                                      (struct sockaddr *)&sender_addr,
                                      &sender_addr_len);

    // 检查接收是否成功
    if (received_bytes < 0)
    {
        LOG_ERROR("Failed to receive box coordinates"); // 接收失败记录错误
        return false;
    }
    else if (received_bytes != sizeof(BOX_RECT))
    {
        LOG_ERROR("Received incomplete box data"); // 接收数据不完整记录错误
        return false;
    }

    // 以下是被注释掉的调试信息代码
    // // 打印接收到的box坐标信息
    // std::stringstream ss;
    // ss << "Received box coordinates - left: " << received_box.left
    //    << ", right: " << received_box.right
    //    << ", top: " << received_box.top
    //    << ", bottom: " << received_box.bottom;
    // LOG_INFO(ss.str());

    // // 打印接收到的box坐标信息
    // std::cout << "Received box coordinates - left: " << received_box.left
    //         << ", right: " << received_box.right
    //         << ", top: " << received_box.top
    //         << ", bottom: " << received_box.bottom << std::endl;

    // 修正：正确的Point2f赋值方式，计算左右下角点的坐标
    left_bottom = cv::Point2f(received_box.left, (received_box.bottom + received_box.top) / 2);   // 左下角点
    right_bottom = cv::Point2f(received_box.right, (received_box.bottom + received_box.top) / 2); // 右下角点

    // TODO: 这里可以添加对接收到的box坐标的处理逻辑
    // 例如将其显示在图像上或进行其他处理

    return true; // 接收成功返回true
}

// 修改主循环中的接收逻辑
// 启动接收数据的线程函数
void WorkFlow::startReceiving()
{
    // 创建UDP socket
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0)
    {
        LOG_ERROR("Failed to create socket"); // 创建socket失败记录错误
        return;
    }

    // 配置接收地址
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));  // 清空结构体
    receiver_addr.sin_family = AF_INET;                // 使用IPv4
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 接受任何IP地址
    receiver_addr.sin_port = htons(8612);              // 使用与发送方相同的端口，这里是8612

    // 绑定socket到本地地址和端口
    if (bind(socket_fd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0)
    {
        LOG_ERROR("Failed to bind socket"); // 绑定socket失败记录错误
        close(socket_fd);                   // 关闭socket
        return;
    }

    struct sockaddr_in sender_addr; // 发送方地址结构
    bool running = true;            // 循环控制变量

    // 循环接收数据
    while (running)
    {
        // 尝试接收box坐标
        if (!receiveBoxCoordinates(socket_fd, sender_addr))
        {
            LOG_WARNING("Failed to receive box coordinates, retrying..."); // 接收失败，记录警告
            std::this_thread::sleep_for(std::chrono::milliseconds(100));   // 暂停100毫秒
            continue;                                                      // 继续下一次循环
        }

        // 处理其他逻辑...
    }

    close(socket_fd); // 关闭socket
}

// WorkFlow类构造函数 - 初始化所有成员变量
WorkFlow::WorkFlow(const Config &config)
    : config_(config),                                                                     // 初始化配置
      shouldStop_(false),                                                                  // 停止标志初始为false
      vicImgWidth_(config.getImageWidth("vic")),                                           // 可见光图像宽度
      vicImgHeight_(config.getImageHeight("vic")),                                         // 可见光图像高度
      irImgWidth_(config.getImageWidth("ir")),                                             // 红外图像宽度
      irImgHeight_(config.getImageHeight("ir")),                                           // 红外图像高度
      networkManager_(config),                                                             // 初始化网络管理器
      g_enc_pipeline(nullptr), g_enc_src(nullptr), g_enc_filter(nullptr),                  // GStreamer编码管道相关指针
      g_encoder(nullptr), g_parse(nullptr), g_stream_sink(nullptr),                        // 编码器、解析器、流接收器指针
      g_encData(nullptr),                                                                  // 编码数据指针
      g_pipeline(nullptr), g_src(nullptr), g_video_filter(nullptr), g_video_sink(nullptr), // 视频管道相关指针
      detector_(config),                                                                   // 初始化探测器
      lidar_thread_running_(false),                                                        // 雷达线程运行标志初始为false
      log_picture_folder_("log_picture"),                                                  // 日志图片文件夹
      left_bottom(0.0f, 0.0f),                                                             // 左下角点初始为原点
      right_bottom(0.0f, 0.0f),                                                            // 右下角点初始为原点
      outputImgWidth_(config.getOutWidth()),                                               // 设置输出宽度
      outputImgHeight_(config.getOutHeight()),
      monitor_thread_running_(false)
{
    // 初始化GStreamer库
    gst_init(nullptr, nullptr);
    LOG_INFO("WorkFlow constructor called"); // 记录构造函数调用
}

// WorkFlow类析构函数 - 释放资源
WorkFlow::~WorkFlow()
{
    stop();            // 停止工作流
    stopLidarThread(); // 停止雷达线程

    // 释放编码数据内存
    if (g_encData != nullptr)
    {
        free(g_encData);
        g_encData = nullptr;
    }

    // 停止并释放编码管道
    if (g_enc_pipeline)
    {
        gst_element_set_state(GST_ELEMENT(g_enc_pipeline), GST_STATE_NULL);
        gst_object_unref(g_enc_pipeline);
    }

    // 停止并释放视频管道
    if (g_pipeline)
    {
        gst_element_set_state(GST_ELEMENT(g_pipeline), GST_STATE_NULL);
        gst_object_unref(g_pipeline);
    }
}

// 初始化工作流 - 设置和启动所有必要的组件
void WorkFlow::initialize()
{
    LOG_INFO("Initializing WorkFlow");                              // 记录初始化开始
    Logger::debug("Loading calibration data and calculating maps"); // 记录校准数据加载

    // 设置默认混合类型
    m_blendType = config_.getDefaultBlendType();
    LOG_INFO("Default blend type set to: " + std::to_string(static_cast<int>(m_blendType)));

    // 加载校准数据并计算映射
    initializeCalibrationAndColorMaps();

    // 进行必要的初始化操作
    networkManager_.initialize(); // 初始化网络管理器

    // 启动接收控制指令的线程
    networkManager_.startReceivingControl([this](const uint8_t *frame, size_t length)
                                          { this->processFormat(frame, length); }); // 使用lambda函数处理接收到的控制帧
    networkManager_.startSendingStatus();                                           // 启动状态发送
    networkManager_.startProcessingVisibleSerial();                                 // 启动可见光串行处理
    networkManager_.startProcessingInfraredSerial();                                // 启动红外串行处理
    networkManager_.startPrepareStatusFrameThread();                                // 启动状态帧准备线程

    // 初始化视频编码管道
    initializeVideoPipeline();

    // 初始化红外光源
    std::string irDevice = config_.getIRVideoDevice(); // 获取IR设备路径
    if (initializeIRSource(irDevice.c_str()))
    {
        LOG_INFO("IR source initialized successfully"); // 红外源初始化成功
    }
    else
    {
        LOG_ERROR("Failed to initialize IR source"); // 红外源初始化失败
    }

    // 初始化可见光源
    std::string visibleDevice = config_.getVisibleVideoDevice(); // 获取可见光设备路径
    if (initializeVisibleSource(visibleDevice.c_str()))
    {
        LOG_INFO("Visible light source initialized successfully"); // 可见光源初始化成功
    }
    else
    {
        LOG_ERROR("Failed to initialize visible light source"); // 可见光源初始化失败
    }

    // 启动编码和推流线程
    encoding_thread_ = std::thread(&WorkFlow::encodingAndStreamingThread, this);
    LOG_INFO("Encoding and streaming thread started"); // 编码和推流线程启动

    // 启动检测线程
    startDetectionThread();
    LOG_INFO("WorkFlow initialized with detection thread"); // 检测线程启动

    // 根据配置决定是否启动雷达线程
    if (config_.getLidarThreadEnabled())
    {
        lidar_thread_running_ = true;
        lidar_thread_ = std::thread(&WorkFlow::lidarThreadFunction, this);
        LOG_INFO("Lidar thread started"); // 雷达线程启动
    }
    else
    {
        LOG_INFO("Lidar thread disabled by configuration"); // 雷达线程被配置禁用
    }

    LOG_INFO("WorkFlow initialized with Lidar thread");

    // 启动接收线程，以分离模式运行（后台运行）
    std::thread receive_thread([this]()
                               { this->startReceiving(); });
    receive_thread.detach(); // 如果希望在后台运行





    // 设置线程监控 - 使用uint64_t替代std::chrono::seconds
    registerThreadMonitor(ThreadType::VISIBLE_CALLBACK, "Visible Light Callback", 
                         5, std::bind(&WorkFlow::reinitializeVisibleSource, this));
    
    registerThreadMonitor(ThreadType::IR_CALLBACK, "Infrared Callback", 
                         5, std::bind(&WorkFlow::reinitializeIRSource, this));
    
    registerThreadMonitor(ThreadType::ENCODING_STREAMING, "Encoding and Streaming", 
                         10, std::bind(&WorkFlow::reinitializeEncodingStreaming, this));
    
    registerThreadMonitor(ThreadType::DETECTION, "Detection", 
                         15, std::bind(&WorkFlow::reinitializeDetection, this));
    
    registerThreadMonitor(ThreadType::LIDAR, "Lidar", 
                         15, std::bind(&WorkFlow::reinitializeLidar, this));
    
    // registerThreadMonitor(ThreadType::NETWORK_CONTROL, "Network Control", 
    //                      30, std::bind(&WorkFlow::reinitializeNetworkControl, this));
    
    // registerThreadMonitor(ThreadType::NETWORK_STATUS, "Network Status", 
    //                      30, std::bind(&WorkFlow::reinitializeNetworkStatus, this));
    
    // 启动监控线程
    startMonitorThread();
    LOG_INFO("Thread monitor started");


    // networkManager_.sendVisibleCtrlSignal(0x63, 0, 0);


    LOG_INFO("WorkFlow initialization completed"); // 工作流初始化完成
}

// 运行工作流 - 主循环
void WorkFlow::run()
{
    LOG_INFO("Running WorkFlow..."); // 记录工作流开始运行

    // 以下是被注释掉的主循环代码
    // // 实现主要的工作流程
    // while (!shouldStop_)
    // {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Add a small delay to prevent CPU hogging
    //     // 执行主循环
    //     // 处理各种任务，如视频处理、LIDAR数据处理等
    // }
}

// 停止工作流 - 清理资源并确保所有线程正确终止
void WorkFlow::stop()
{
    if (!shouldStop_) // 如果尚未停止
    {
        LOG_INFO("Stopping WorkFlow..."); // 记录停止过程开始
        shouldStop_ = true;               // 设置停止标志
        networkManager_.stop();           // 停止网络管理器

        stopDetectionThread(); // 停止检测线程
        stopLidarThread();     // 停止雷达线程
        stopMonitorThread();  // 停止监控线程

        // 等待编码线程结束
        if (encoding_thread_.joinable())
        {
            encoding_thread_.join(); // 等待编码线程完成
        }

        LOG_INFO("WorkFlow stopped"); // 记录工作流已停止
    }
}

// 在图像上绘制雷达手动选择区域
void WorkFlow::drawLidarManualSelection(cv::Mat &mat_image, const LidarManualSelectionContext &selection)
{
    // 仅当激活模式且选择不为空时绘制
    if (selection.ctrl.mode == LidarManualSelectionMode::ACTIVATE && !selection.empty)
    {
        // 在图像上绘制矩形框
        cv::rectangle(mat_image, selection.roi.rect, selection.formatted_txt.style.color, 2);

        // 增加 y 坐标以将文本往下移动
        auto origin = cv::Point(32, selection.formatted_txt.size.height + 64); // 将 32 改为 64

        // 在图像上绘制文本
        cv::putText(mat_image, selection.formatted_txt.label, origin, selection.formatted_txt.style.face,
                    selection.formatted_txt.style.scale, selection.formatted_txt.style.color,
                    selection.formatted_txt.style.thickness,
                    cv::LINE_8);
    }
}

// 更新分辨率 - 重新创建编码管道以适应新的分辨率
bool WorkFlow::updateResolution(int new_width, int new_height)
{
    try
    {
        LOG_INFO("Updating resolution to: " + std::to_string(new_width) + "x" + std::to_string(new_height)); // 记录分辨率更新

        // 停止并清理现有管道
        if (g_enc_pipeline)
        {
            gst_element_set_state(GST_ELEMENT(g_enc_pipeline), GST_STATE_NULL); // 停止管道
            gst_object_unref(g_enc_pipeline);                                   // 释放资源
            g_enc_pipeline = nullptr;                                           // 置空指针
        }

        // 更新内部分辨率记录
        outputImgWidth_ = new_width;
        outputImgHeight_ = new_height;

        // 重新创建编码管道
        int currentBpsRate = config_.getInitialBpsRate(); // 获取当前码率或初始码率
        if (!createEncoderPipeline(currentBpsRate))
        {
            LOG_ERROR("Failed to recreate encoder pipeline"); // 重新创建编码管道失败
            return false;
        }

        // 启动新管道 - 首先设置为暂停状态
        std::this_thread::sleep_for(std::chrono::seconds(1));                 // 等待1秒
        gst_element_set_state(GST_ELEMENT(g_enc_pipeline), GST_STATE_PAUSED); // 设置为暂停状态
        std::this_thread::sleep_for(std::chrono::seconds(1));                 // 再等待1秒

        // 设置为播放状态
        GstStateChangeReturn ret = gst_element_set_state(GST_ELEMENT(g_enc_pipeline), GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE)
        {
            LOG_ERROR("Failed to set new pipeline to PLAYING state"); // 设置为播放状态失败
            return false;
        }

        LOG_INFO("Resolution updated successfully with new pipeline"); // 记录分辨率更新成功
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in updateResolution: " + std::string(e.what())); // 记录异常
        return false;
    }
}

// 验证显示模式和IP地址是否匹配
bool WorkFlow::validateDisplayModeAndIP(uint8_t display_mode, const std::string &localIP)
{
    // 创建显示模式和对应的IP地址映射表
    const std::unordered_map<uint8_t, std::string> mode_ip_mapping = {
        {0b010, "192.168.127.100"}, // 船艏 Bow
        {0b011, "192.168.127.110"}, // 右舷 Starboard
        {0b100, "192.168.127.120"}, // 船艉 Stern
        {0b101, "192.168.127.130"}  // 左舷 Port
    };

    // 检查显示模式是否存在于映射表中
    auto it = mode_ip_mapping.find(display_mode);
    if (it == mode_ip_mapping.end())
    {
        // LOG_WARNING("Invalid display mode: " + std::to_string(display_mode)); // 无效的显示模式
        return false;
    }

    // 比较IP地址是否匹配
    bool matches = (it->second == localIP);

    // // 记录匹配结果
    // if (!matches)
    // {
    //     LOG_WARNING("Display mode and IP mismatch - Mode: " + std::to_string(display_mode) +
    //                 ", Expected IP: " + it->second + ", Actual IP: " + localIP); // 显示模式和IP不匹配
    // }
    // else
    // {
    //     LOG_INFO("Display mode and IP match validated successfully"); // 显示模式和IP匹配成功
    // }

    return matches;
}

// 注释掉的设置输出尺寸函数
// void WorkFlow::setOutputDimensions() {

//     // Set output dimensions based on display mode
//     if (display_mode == 0b010 ||    // 船艏(单幅显示）Bow
//         display_mode == 0b011 ||    // 右舷(单幅显示）Starboard
//         display_mode == 0b100 ||    // 船艉(单幅显示）Stern
//         display_mode == 0b101) {    // 左舷(单幅显示）Port

//         single_mode = true;

//         outputImgWidth_ = 1920;
//         outputImgHeight_ = 1080;

//         setEncoderBps(4000000);
//     } else {
//         outputImgWidth_ = 960;
//         outputImgHeight_ = 540;

//         setEncoderBps(4000000);

//         single_mode = false;
//     }

//     // 在需要更新分辨率时调用
//     if (!updateResolution(outputImgWidth_, outputImgHeight_)) {
//         LOG_ERROR("Failed to update resolution");
//     }

// }
// 根据显示模式设置输出尺寸
void WorkFlow::setOutputDimensions()
{
    // 根据显示模式设置输出尺寸
    if (display_mode == 0b010 || // 船艏(单幅显示）Bow
        display_mode == 0b011 || // 右舷(单幅显示）Starboard
        display_mode == 0b100 || // 船艉(单幅显示）Stern
        display_mode == 0b101)   // 左舷(单幅显示）Port
    {
        // 设置为单一模式
        single_mode = true;

        // 如果当前分辨率不是1920x1080，则更新
        if (outputImgWidth_ != 1920 || outputImgHeight_ != 1080)
        {
            outputImgWidth_ = 1920;  // 设置宽度为1920
            outputImgHeight_ = 1080; // 设置高度为1080

            // 调用更新分辨率函数，如果失败则记录错误
            if (!updateResolution(outputImgWidth_, outputImgHeight_))
            {
                LOG_ERROR("Failed to update resolution");
            }
        }

        // setEncoderBps(4000000);  // 已注释掉的码率设置
    }
    else
    {
        // 如果当前分辨率不是960x540，则更新
        if (outputImgWidth_ != 960 || outputImgHeight_ != 540)
        {
            outputImgWidth_ = 960;  // 设置宽度为960
            outputImgHeight_ = 540; // 设置高度为540

            // 调用更新分辨率函数，如果失败则记录错误
            if (!updateResolution(outputImgWidth_, outputImgHeight_))
            {
                LOG_ERROR("Failed to update resolution");
            }
        }

        // setEncoderBps(4000000);  // 已注释掉的码率设置

        single_mode = false; // 设置为非单一模式
    }
}

// 处理控制帧格式并执行相应操作
void WorkFlow::processFormat(const uint8_t *control_frame, size_t frame_length)
{
    LOG_DEBUG("Processing format, frame length: " + std::to_string(frame_length)); // 记录调试信息

    // 验证帧头和长度
    if (frame_length != 36 && frame_length != 14)
    {
        LOG_ERROR("Invalid frame length: " + std::to_string(frame_length)); // 记录无效帧长度错误
        return;
    }

    // 验证帧头内容
    if (control_frame[0] != 0x27 ||
        (control_frame[1] != 0x01 && control_frame[1] != 0x03) ||
        (frame_length == 36 && control_frame[2] != 0x24) ||
        (frame_length == 14 && control_frame[2] != 0x0e))
    {
        LOG_ERROR("Invalid frame header"); // 记录无效帧头错误
        return;
    }

    // 验证校验和
    uint8_t calculatedChecksum = Utils::calculateChecksum(control_frame, frame_length - 1);
    if (calculatedChecksum != control_frame[frame_length - 1])
    {
        LOG_ERROR("Checksum mismatch"); // 记录校验和不匹配错误
        return;
    }

    // 处理公共字段
    selftest = control_frame[3] & 0x01; // 提取自检标志位

    // fusion_format = control_frame[4] & 0x07;  // 已注释掉的代码

    // 提取显示模式 (D2D1D0)
    display_mode = control_frame[4] & 0x07; // 低3位为显示模式

    // 提取光学模式 (D5D4D3)
    fusion_format = (control_frame[4] >> 3) & 0x07; // 位4-6为融合模式

    detection_format = control_frame[25] & 0x03; // 提取检测格式（低2位）

    // 已注释掉的条件判断
    // if (control_frame[25] & 0x03 != 0x00)
    // {
    //     detection_format = control_frame[25] & 0x03;
    // }

    // 根据检测格式设置相应模式
    if (detection_format == 0x01)
    {
        irNeedForDetection = false; // IR不需要用于检测
        LOG_INFO("识别模式：白天"); // 记录白天模式
    }
    else if (detection_format == 0x02)
    {
        irNeedForDetection = true;  // IR需要用于检测
        LOG_INFO("识别模式：夜晚"); // 记录夜晚模式
    }
    else
    {
        LOG_INFO("识别模式：保持"); // 记录保持当前模式
    }

    // 以下是已注释掉的调试打印代码
    // // 打印提取的模式值
    // printf("显示模式(D2D1D0): %d\n", display_mode);
    // printf("光学模式(D5D4D3): %d\n", fusion_format);

    // // 更详细的调试信息
    // printf("\n详细信息:\n");
    // printf("原始字节值: 0x%02X\n", control_frame[4]);
    // printf("显示模式位(D2D1D0): 0b%03d\n", display_mode);
    // printf("光学模式位(D5D4D3): 0b%03d\n", fusion_format);

    // 设置网络管理器的混合状态
    networkManager_.setBlendStatusState(fusion_format);

    // 以下是已注释掉的根据显示模式设置输出尺寸的代码
    // // Set output dimensions based on display mode
    // if (display_mode == 0b010 ||    // 船艏(单幅显示）Bow
    //     display_mode == 0b011 ||    // 右舷(单幅显示）Starboard
    //     display_mode == 0b100 ||    // 船艉(单幅显示）Stern
    //     display_mode == 0b101) {    // 左舷(单幅显示）Port

    //     outputImgWidth_ = 1920;
    //     outputImgHeight_ = 1080;
    // } else {
    //     outputImgWidth_ = 960;
    //     outputImgHeight_ = 540;
    // }

    // 调用设置输出尺寸函数
    setOutputDimensions();

    // 如果帧长度为36，处理Format36特定字段
    if (frame_length == 36)
    {
        // 处理Format36特定字段
        ir_control_code = control_frame[5];                             // 红外控制代码
        vic_control_code = control_frame[6];                            // 可见光控制代码
        stabilization_mode = control_frame[7] & 0x03;                   // 稳定模式（低2位）
        encode_rate = control_frame[8] & 0x07;                          // 编码率（低3位）
        reboot = control_frame[9] & 0x03;                               // 重启标志（低2位）
        log_save = control_frame[10] & 0x03;                            // 日志保存标志（低2位）
        ota_upgrade_display = control_frame[11] & 0x01;                 // OTA升级显示标志（最低位）
        ota_upgrade_start = (control_frame[11] >> 1) & 0x01;            // OTA升级开始标志（第2位）
        ota_version = control_frame[12];                                // OTA版本
        focus_area_low = (control_frame[14] << 8) | control_frame[13];  // 焦点区域低字节
        focus_area_high = (control_frame[16] << 8) | control_frame[15]; // 焦点区域高字节
        char_overlay = control_frame[17] & 0x01;                        // 字符叠加标志（最低位）
        need_flip_horizontally = control_frame[18];                     // 是否需要水平翻转

        // 执行Format36特定控制操作
        uint16_t crosshairX = 0, crosshairY = 0;                                         // 假设在其他地方定义了这些变量
        networkManager_.sendVisibleCtrlSignal(vic_control_code, crosshairX, crosshairY); // 发送可见光控制信号
        networkManager_.sendInfraredCtrlSignal(ir_control_code, crosshairX, crosshairY); // 发送红外控制信号
        WorkFlowUtils::handleLogSave(log_save);                                          // 处理日志保存
        WorkFlowUtils::handleOtaUpgradeDisplay(ota_upgrade_display);                     // 处理OTA升级显示
        WorkFlowUtils::handleOtaUpgradeStart(ota_upgrade_start);                         // 处理OTA升级开始
        WorkFlowUtils::handleOtaVersion(ota_version);                                    // 处理OTA版本
        WorkFlowUtils::handleFocusAreaLow(focus_area_low);                               // 处理焦点区域低字节
        WorkFlowUtils::handleFocusAreaHigh(focus_area_high);                             // 处理焦点区域高字节
        WorkFlowUtils::updateEnableDetection(char_overlay);                              // 更新启用检测状态

        // 雷达手动框选测距功能
        auto lidar_manual_selection_mode = static_cast<LidarManualSelectionMode>(control_frame[19]); // 获取雷达手动选择模式
        if (lidar_manual_selection_mode != LidarManualSelectionMode::NO_ACTION)                      // 如果不是无动作
        {
            lidar_manual_selection_ctx.empty = true;                                                         // 设置为空
            lidar_manual_selection_ctx.ctrl.mode = lidar_manual_selection_mode;                              // 设置模式
            lidar_manual_selection_ctx.ctrl.x = *((uint16_t *)(control_frame + 20));                         // 设置X坐标
            lidar_manual_selection_ctx.ctrl.y = *((uint16_t *)(control_frame + 22));                         // 设置Y坐标
            lidar_manual_selection_ctx.ctrl.size = static_cast<LidarManualSelectionSize>(control_frame[24]); // 设置大小
        }
    }
    else if (frame_length == 14) // 如果帧长度为14，处理Format14特定字段
    {
        task_mode = control_frame[4] & 0x03;          // 任务模式
        stabilization_mode = control_frame[5] & 0x03; // 稳定模式
        encode_rate = control_frame[6] & 0x07;        // 编码率
        reboot = control_frame[7] & 0x03;             // 重启标志
    }

    // 执行公共控制操作
    WorkFlowUtils::startSelfTest(selftest);                     // 启动自检
    updateBlendType(fusion_format, m_blendType);                // 更新混合类型
    WorkFlowUtils::switchStabilizationMode(stabilization_mode); // 切换稳定模式


    
    updateEncodeRate(encode_rate);            // 更新编码率



    // std::cout << "Stop update encode rate." << std::endl; // 输出创建成功信息

    
    WorkFlowUtils::handleReboot(reboot);      // 处理重启
    LOG_DEBUG("Format processing completed"); // 记录格式处理完成
}

// 初始化校准数据和颜色映射
void WorkFlow::initializeCalibrationAndColorMaps()
{
    Utils::bind_to_cpus({1});                                       // 绑定到CPU 1
    Logger::debug("Loading calibration data and calculating maps"); // 记录正在加载校准数据

    // 从Config获取校准数据
    cv::Mat K_vic = config_.getVicUndistortionK(); // 获取可见光校正K矩阵
    cv::Mat D_vic = config_.getVicUndistortionD(); // 获取可见光畸变系数
    cv::Mat K_ir = config_.getIrUndistortionK();   // 获取红外校正K矩阵
    cv::Mat D_ir = config_.getIrUndistortionD();   // 获取红外畸变系数
    cv::Mat H = config_.getRegistrationH();        // 获取配准H矩阵

    // 检查所有必要的矩阵是否都已加载
    if (K_vic.empty() || D_vic.empty() || K_ir.empty() || D_ir.empty() || H.empty())
    {
        Logger::error("Calibration data is incomplete. Cannot calculate calibration maps."); // 记录校准数据不完整错误
        return;
    }

    // 计算可见光图像的畸变校正映射
    cv::initUndistortRectifyMap(K_vic, D_vic, cv::Mat(), K_vic,
                                cv::Size(vicImgWidth_, vicImgHeight_), CV_32FC1,
                                vic_map1_, vic_map2_);
    Logger::debug("Visible light undistortion map calculated"); // 记录可见光畸变校正映射已计算

    // 计算红外图像的畸变校正映射
    cv::initUndistortRectifyMap(K_ir, D_ir, cv::Mat(), K_ir,
                                cv::Size(irImgWidth_, irImgHeight_), CV_32FC1,
                                ir_map1_, ir_map2_);
    Logger::debug("Infrared undistortion map calculated"); // 记录红外畸变校正映射已计算

    // 计算红外到可见光图像的重映射坐标
    cv::initUndistortRectifyMap(K_ir, D_ir, cv::Mat(), H * K_ir,
                                cv::Size(vicImgWidth_, vicImgHeight_), CV_32FC1,
                                map_x_, map_y_);
    Logger::debug("Infrared to visible light remapping coordinates calculated"); // 记录红外到可见光重映射坐标已计算

    // 从配置获取IR映射方法
    bool usePseudoColorMapping = (config_.getIRMappingMethod() == "pseudocolor"); // 判断是否使用伪彩色映射

    // 预计算IR到颜色的映射
    for (int i = 0; i < 256; i++)
    {
        cv::Vec3b ir_value(i, i, i); // 创建灰度向量
        if (usePseudoColorMapping)
        {
            ir2ColorMap[i] = Utils::mapIRToPseudoColor(ir_value); // 使用伪彩色映射
        }
        else
        {
            ir2ColorMap[i] = Utils::mapIRToGreenSpace(ir_value); // 使用绿色空间映射
        }
    }
    LOG_DEBUG(std::string("IR to color map precomputed using ") +
              (usePseudoColorMapping ? "pseudo color" : "green space") +
              " mapping"); // 记录使用了哪种映射方法

    // 创建综合映射矩阵
    combined_map_x_.create(cv::Size(vicImgWidth_, vicImgHeight_), CV_32FC1);
    combined_map_y_.create(cv::Size(vicImgWidth_, vicImgHeight_), CV_32FC1);

    // 计算每个像素点的综合映射值
    for (int y = 0; y < vicImgHeight_; ++y)
    {
        for (int x = 0; x < vicImgWidth_; ++x)
        {
            // 使用 map_x 和 map_y 中的值反向查找对应的红外图像坐标
            float x_ir = map_x_.at<float>(y, x);
            float y_ir = map_y_.at<float>(y, x);

            // 确保坐标在红外图像范围内
            if (x_ir >= 0 && x_ir < irImgWidth_ && y_ir >= 0 && y_ir < irImgHeight_)
            {
                // 使用红外图像的畸变校正映射
                combined_map_x_.at<float>(y, x) = ir_map1_.at<float>(static_cast<int>(y_ir), static_cast<int>(x_ir));
                combined_map_y_.at<float>(y, x) = ir_map2_.at<float>(static_cast<int>(y_ir), static_cast<int>(x_ir));
            }
            else
            {
                // 处理越界情况
                combined_map_x_.at<float>(y, x) = -1;
                combined_map_y_.at<float>(y, x) = -1;
            }
        }
    }

    // 预计算距离到颜色的映射
    for (int i = 0; i < 256; i++)
    {
        dist2ColorMap[i] = Utils::mapDistanceToColor(i); // 计算距离对应的颜色
    }

    LOG_DEBUG("Distance to color map precomputed"); // 记录距离颜色映射已预计算

    LOG_INFO("Combined remapping coordinates calculation completed"); // 记录综合重映射坐标计算完成

    LOG_INFO("Calibration and color maps initialization completed."); // 记录校准和颜色映射初始化完成
}

// 初始化视频管道
void WorkFlow::initializeVideoPipeline()
{
    Utils::bind_to_cpus({1, 2});                                   // 绑定到CPU 1和2
    Logger::info("Thread-initializeVideoPipeline bound to CPU 1"); // 记录线程绑定信息

    LOG_INFO("Initializing video encoding pipeline..."); // 记录正在初始化视频编码管道

    int bpsRate = config_.getInitialBpsRate(); // 获取初始比特率

    // 创建编码器管道
    if (createEncoderPipeline(bpsRate))
    {
        std::cout << "Encoder pipeline created successfully." << std::endl; // 输出创建成功信息
    }
    else
    {
        std::cerr << "Failed to create encoder pipeline." << std::endl; // 输出创建失败信息
        return;
    }

    // 检查管道是否为空
    if (GST_ELEMENT(g_enc_pipeline) == nullptr)
    {
        std::cerr << "Encoder pipeline is null." << std::endl; // 输出管道为空错误
        return;
    }
    else
    {
        std::cout << "Encoder pipeline is not null." << std::endl; // 输出管道非空信息
    }

    // 设置管道状态
    std::this_thread::sleep_for(std::chrono::seconds(1));                  // 等待1秒
    gst_element_set_state(GST_ELEMENT(g_enc_pipeline), GST_STATE_PAUSED);  // 设置为暂停状态
    std::this_thread::sleep_for(std::chrono::seconds(1));                  // 再等待1秒
    gst_element_set_state(GST_ELEMENT(g_enc_pipeline), GST_STATE_PLAYING); // 设置为播放状态
    LOG_INFO("Video encoding pipeline initialized and started.");          // 记录视频编码管道已初始化并启动
}

// 更新混合类型
void WorkFlow::updateBlendType(uint8_t fusion_format, BlendType &m_blendType)
{
    LOG_INFO("Updating blend type with fusion format: 0x" + std::to_string(fusion_format)); // 记录更新混合类型

    // 定义混合类型映射表
    static const std::array<std::pair<uint8_t, BlendType>, 7> blendTypeMap = {{{0x00, BlendType::V},
                                                                               {0x01, BlendType::I},
                                                                               {0x02, BlendType::VL},
                                                                               {0x03, BlendType::IL},
                                                                               {0x04, BlendType::VI},
                                                                               {0x05, BlendType::VIL},
                                                                               {0x07, BlendType::L}}};

    // 查找对应的混合类型
    auto it = std::find_if(blendTypeMap.begin(), blendTypeMap.end(),
                           [fusion_format](const auto &pair)
                           { return pair.first == fusion_format; });

    // 如果找到则更新混合类型，否则默认为V类型
    if (it != blendTypeMap.end())
    {
        m_blendType = it->second;
        LOG_INFO("Blend type updated successfully to: " + std::to_string(static_cast<int>(m_blendType))); // 记录更新成功
    }
    else
    {
        m_blendType = BlendType::V;
        LOG_INFO("Invalid fusion format. Defaulting to blend type V"); // 记录默认为V类型
    }

    LOG_INFO("Final blend type: " + std::to_string(static_cast<int>(m_blendType))); // 记录最终混合类型
}

// 更新编码率
void WorkFlow::updateEncodeRate(uint8_t encode_rate)
{
    LOG_INFO("Updating encode rate to: " + std::to_string(encode_rate)); // 记录更新编码率
    switch (encode_rate)
    {
    case 0x00:
        setEncoderBps(4000000); // 4Mbps
        break;
    case 0x01:
        setEncoderBps(2000000); // 2Mbps
        break;
    case 0x02:
        setEncoderBps(1000000); // 1Mbps
        break;
    case 0x03:
        setEncoderBps(500000); // 500kbps
        break;
    default:
        LOG_WARNING("Invalid encode rate. Defaulting to 500000 bps."); // 记录无效编码率警告
        setEncoderBps(500000);                                         // 默认500kbps
        break;
    }
}

// 创建可见光视频源
bool WorkFlow::createVisibleVideoSource(int width, int height, const char *device, GstAppSinkCallbacks callbacks)
{
    // 创建GStreamer管道和元素
    g_pipeline = GST_PIPELINE(gst_pipeline_new(NULL));             // 创建管道
    g_src = gst_element_factory_make("v4l2src", NULL);             // 创建V4L2源
    g_video_filter = gst_element_factory_make("capsfilter", NULL); // 创建过滤器
    g_video_sink = gst_element_factory_make("appsink", NULL);      // 创建接收器

    // 检查元素是否创建成功
    if (!g_pipeline || !g_src || !g_video_filter || !g_video_sink)
    {
        LOG_ERROR("Video capture pipeline elements create failed!"); // 记录元素创建失败
        return false;
    }

    LOG_INFO("Video capture pipeline elements created successfully"); // 记录元素创建成功

    // 设置设备
    g_object_set(G_OBJECT(g_src), "device", device, NULL);

    // 创建caps配置
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "width", G_TYPE_INT, width,
                                        "height", G_TYPE_INT, height,
                                        "format", G_TYPE_STRING, "NV12",
                                        "framerate", GST_TYPE_FRACTION, 25, 1,
                                        NULL);

    // 设置过滤器caps
    g_object_set(G_OBJECT(g_video_filter), "caps", caps, NULL);
    gst_caps_unref(caps); // 释放caps引用

    // 设置接收器属性
    g_object_set(G_OBJECT(g_video_sink), "emit-signals", TRUE, "async", FALSE, NULL);

    // 获取新样本回调函数
    GstFlowReturn (*new_sample_callback)(GstAppSink *, gpointer) = callbacks.new_sample;

    // 连接信号
    g_signal_connect(G_OBJECT(g_video_sink), "new-sample", G_CALLBACK(new_sample_callback), this);

    // 将元素添加到管道中
    gst_bin_add_many(GST_BIN(g_pipeline), g_src, g_video_filter, g_video_sink, NULL);

    // 链接元素
    if (!gst_element_link_many(g_src, g_video_filter, g_video_sink, NULL))
    {
        LOG_ERROR("Video pipeline link element failed!"); // 记录链接元素失败
        return false;
    }

    // 设置管道为播放状态
    gst_element_set_state(GST_ELEMENT(g_pipeline), GST_STATE_PLAYING);
    LOG_INFO("Video capture pipeline set to PLAYING state"); // 记录设置为播放状态
    return true;
}

// 初始化红外源
bool WorkFlow::initializeIRSource(const char *device)
{
    // 已注释掉的CPU绑定代码
    // Utils::bind_to_cpus({1});
    // Logger::info("Thread-initializeIRSource bound to CPU 1");

    // 创建红外回调
    GstAppSinkCallbacks ir_callbacks = {NULL, NULL, irSinkCallback, NULL};

    // 调用创建可见光视频源函数
    return createVisibleVideoSource(irImgWidth_, irImgHeight_, device, ir_callbacks);
}

// 初始化可见光源
bool WorkFlow::initializeVisibleSource(const char *device)
{
    Utils::bind_to_cpus({1});                                      // 绑定到CPU 1
    Logger::info("Thread-initializeVisibleSource bound to CPU 1"); // 记录线程绑定信息

    // 创建可见光回调
    GstAppSinkCallbacks vic_callbacks = {NULL, NULL, vicSinkCallback, NULL};

    // 调用创建可见光视频源函数
    return createVisibleVideoSource(vicImgWidth_, vicImgHeight_, device, vic_callbacks);
}


// 红外图像处理回调函数，从GStreamer接收红外图像并进行处理
GstFlowReturn WorkFlow::irSinkCallback(GstAppSink *appsink, gpointer user_data)
{
    WorkFlow *workflow = static_cast<WorkFlow *>(user_data); // 将用户数据转换为WorkFlow对象指针
    
    // 更新线程时间戳
    workflow->updateThreadTimestamp(ThreadType::IR_CALLBACK);

    GstSample *sample = pullSample(appsink);                 // 从appsink获取样本
    if (!sample)                                             // 如果没有获取到样本，直接返回
        return GST_FLOW_OK;

    GstBuffer *buffer = gst_sample_get_buffer(sample); // 从样本中获取缓冲区
    GstMapInfo mapinfo;                                // 用于映射缓冲区的结构
    if (!mapBuffer(buffer, mapinfo))                   // 映射缓冲区，如果失败则释放样本并返回
    {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    int width, height;                   // 声明宽度和高度变量
    getCapsInfo(appsink, width, height); // 从appsink的caps中获取宽度和高度

    // 计算并检查所需的缓冲区大小（YUV420格式，需要width*height*1.5的空间）
    size_t kRequiredBufSize = workflow->irImgWidth_ * workflow->irImgHeight_ * 3 / 2;
    if (mapinfo.size != kRequiredBufSize)
    {
        LOG_ERROR("mapinfo.size == " + std::to_string(mapinfo.size) + ", but it must be equal to " + std::to_string(kRequiredBufSize));
    }

    // 检查是否需要处理IR图像，根据当前的混合类型决定
    bool needProcessIR = (workflow->m_blendType == BlendType::I ||   // 纯红外模式
                          workflow->m_blendType == BlendType::IL ||  // 红外+激光模式
                          workflow->m_blendType == BlendType::VIL || // 可见光+红外+激光模式
                          workflow->m_blendType == BlendType::VI);   // 可见光+红外模式

    // 如果不需要处理红外图像，则休眠40毫秒后返回
    if (!needProcessIR)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(40)); // 添加休眠
        gst_buffer_unmap(buffer, &mapinfo);                         // 解除缓冲区映射
        gst_sample_unref(sample);                                   // 释放样本引用
        return GST_FLOW_OK;
    }
    
    // 计算目标图像大小（BGRA格式，每像素4字节）
    int dst_size = workflow->irImgWidth_ * workflow->irImgHeight_ * 4;

    // 计算目标图像大小
    int dst_width = workflow->irImgWidth_;
    int dst_height = workflow->irImgHeight_;

    try
    {
        // // 使用RGA硬件加速调整图像大小和格式（从YUV420转为BGRA）
        // unsigned char *ir_data = RGAOperations::resizeImage(
        //     mapinfo.data, width, height, RK_FORMAT_YCbCr_420_SP,                           // 源数据、宽度、高度和格式
        //     workflow->irImgWidth_, workflow->irImgHeight_, RK_FORMAT_BGRA_8888, dst_size, "irSinkCallback"); // 目标宽度、高度、格式和大小



        // 创建一个临时的OpenCV Mat来包装原始YUV数据
        cv::Mat yuv_image(height * 3 / 2, width, CV_8UC1, mapinfo.data);

        // 将YUV420格式转换为BGRA
        cv::Mat bgra_image;
        cv::cvtColor(yuv_image, bgra_image, cv::COLOR_YUV2BGRA_YV12);

        // 将图像调整为目标大小
        cv::Mat resized_image;
        cv::resize(bgra_image, resized_image, cv::Size(dst_width, dst_height), 0, 0, cv::INTER_LINEAR);

        // 创建一个指向调整大小后图像数据的指针
        unsigned char* ir_data = resized_image.data;


        if (ir_data) // 如果成功获取了调整大小的图像数据
        {
            // 创建OpenCV矩阵包装图像数据
            cv::Mat ir_mat(height, width, CV_8UC4, (void *)ir_data);
            cv::Mat aligned_ir; // 用于存储校准后的图像



            // cv::Mat bgr_img;
            // cv::cvtColor(ir_mat, bgr_img, cv::COLOR_BGRA2BGR);


            // // 生成基于时间戳的唯一文件名
            // auto now = std::chrono::system_clock::now();
            // auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            //     now.time_since_epoch()).count();

            // std::string filename = "./debug_images/infrared_image_" + 
            //                     std::to_string(timestamp) + ".jpg";
                                

            // cv::imwrite(filename, bgr_img);



            
            // cv::remap(ir_mat, aligned_ir, workflow->ir_map1_, workflow->ir_map2_, cv::INTER_NEAREST, cv::BORDER_CONSTANT);

            // cv::imwrite("./undistorted_ir_image.jpg", aligned_ir);
            // // cv::remap(undistorted_ir_umat, undistorted_ir_umat, workflow->map_x_, workflow->map_y_, cv::INTER_NEAREST, cv::BORDER_CONSTANT);

            // 使用校准映射对图像进行重映射校准
            cv::remap(ir_mat, aligned_ir, workflow->combined_map_x_, workflow->combined_map_y_,
                      cv::INTER_NEAREST, cv::BORDER_CONSTANT);

            // 如果需要用红外图像进行检测，更新detection_image
            if (workflow->irNeedForDetection)
            {
                // 以下是被注释掉的调试打印代码
                // // 流输出打印，红外检测模式
                // std::cout << "=======================================" << std::endl;
                // std::cout << "     Infrared light detection mode: " <<  std::endl;
                // std::cout << "=======================================" << std::endl;

                // 加锁更新检测图像
                std::lock_guard<std::mutex> lock(workflow->detection_image_mutex_);
                workflow->detection_image = aligned_ir.clone();
            }

            // 处理纯红外模式(I)或红外+激光模式(IL)
            if (workflow->m_blendType == BlendType::I || workflow->m_blendType == BlendType::IL)
            {
                // 加锁更新流输出图像
                std::lock_guard<std::mutex> lock(workflow->stream_image_mutex_);
                workflow->stream_image = aligned_ir.clone();
            }
            // 处理可见光+红外+激光模式(VIL)或可见光+红外模式(VI)
            else if (workflow->m_blendType == BlendType::VIL || workflow->m_blendType == BlendType::VI)
            {
                // 创建伪彩色红外图像
                cv::Mat pseudo_color_ir(aligned_ir.size(), CV_8UC4);

                // 使用OpenCV并行处理将灰度红外图像转换为伪彩色
                cv::parallel_for_(cv::Range(0, aligned_ir.rows), [&](const cv::Range &range)
                                  {
                    for (int y = range.start; y < range.end; ++y)
                    {
                        const cv::Vec4b* src = aligned_ir.ptr<cv::Vec4b>(y);
                        cv::Vec4b* dst = pseudo_color_ir.ptr<cv::Vec4b>(y);
                        for (int x = 0; x < aligned_ir.cols; ++x)
                        {
                            cv::Vec3b color = workflow->ir2ColorMap[src[x][0]];
                            dst[x] = cv::Vec4b(color[0], color[1], color[2], src[x][3]);
                        }
                    } });

                // 加锁更新用于融合的红外图像
                std::lock_guard<std::mutex> lock(workflow->aligned_ir_for_fusion_mutex_);
                workflow->aligned_ir_for_fusion = pseudo_color_ir.clone();
            }

            // delete[] ir_data; // 释放通过RGA分配的内存
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Image conversion failed: " << e.what() << std::endl; // 记录图像转换失败的异常
    }

    // 清理GStreamer资源
    gst_buffer_unmap(buffer, &mapinfo); // 解除缓冲区映射
    gst_sample_unref(sample);           // 释放样本引用

    LOG_DEBUG("Infrade light sink callback called"); // 记录回调函数调用
    // std::this_thread::sleep_for(std::chrono::milliseconds(40));  // 被注释掉的休眠代码

    return GST_FLOW_OK; // 返回成功状态
}

// 可见光图像处理回调函数，从GStreamer接收可见光图像并进行处理
GstFlowReturn WorkFlow::vicSinkCallback(GstAppSink *appsink, gpointer user_data)
{
    WorkFlow *workflow = static_cast<WorkFlow *>(user_data); // 将用户数据转换为WorkFlow对象指针

    // 更新线程时间戳
    workflow->updateThreadTimestamp(ThreadType::VISIBLE_CALLBACK);

    GstSample *sample = pullSample(appsink);                 // 从appsink获取样本
    if (!sample)                                             // 如果没有获取到样本，直接返回
        return GST_FLOW_OK;

    GstBuffer *buffer = gst_sample_get_buffer(sample); // 从样本中获取缓冲区
    GstMapInfo mapinfo;                                // 用于映射缓冲区的结构
    if (!mapBuffer(buffer, mapinfo))                   // 映射缓冲区，如果失败则释放样本并返回
    {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    int width, height;                   // 声明宽度和高度变量
    getCapsInfo(appsink, width, height); // 从appsink的caps中获取宽度和高度

    // 计算并检查所需的缓冲区大小（YUV420格式，需要width*height*1.5的空间）
    size_t kRequiredBufSize = workflow->vicImgWidth_ * workflow->vicImgHeight_ * 3 / 2;
    if (mapinfo.size != kRequiredBufSize)
    {
        LOG_ERROR("mapinfo.size == " + std::to_string(mapinfo.size) + ", but it must be equal to " + std::to_string(kRequiredBufSize));
    }

    // 检查是否需要处理可见光图像，根据当前的混合类型决定
    bool needProcessVic = (workflow->m_blendType == BlendType::V ||   // 纯可见光模式
                           workflow->m_blendType == BlendType::VL ||  // 可见光+激光模式
                           workflow->m_blendType == BlendType::VIL || // 可见光+红外+激光模式
                           workflow->m_blendType == BlendType::VI);   // 可见光+红外模式

    // 以下是被注释掉的代码，原本用于检查是否需要用于检测
    // // 检查是否需要用于检测
    // bool needForDetection = (workflow->m_blendType != BlendType::L);
    // 如果detection_format为0x01，则需要处理可见光图像和用于检测

    // bool needForDetection;

    // if (workflow->detection_format == 0x01)
    // {
    //     needForDetection = true;
    // }else if (workflow->detection_format == 0x02)
    // {
    //     needForDetection = false;
    // }

    // // 流输出打印，可见光检测模式
    // std::cout << "Visible light detection mode test:" << needForDetection << std::endl;

    // // 如果都不需要，直接返回
    // if (!needProcessVic && !needForDetection)
    // {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(40)); // 添加40ms休眠
    //     gst_buffer_unmap(buffer, &mapinfo);
    //     gst_sample_unref(sample);
    //     return GST_FLOW_OK;
    // }

    // 计算目标图像大小（BGRA格式，每像素4字节）
    int dst_size = workflow->vicImgWidth_ * workflow->vicImgHeight_ * 4;

    // 计算目标图像大小
    int dst_width = workflow->vicImgWidth_;
    int dst_height = workflow->vicImgHeight_;
    
    try
    {
        // // 使用RGA硬件加速调整图像大小和格式（从YUV420转为BGRA）
        // unsigned char *vic_data = RGAOperations::resizeImage(
        //     mapinfo.data, width, height, RK_FORMAT_YCbCr_420_SP,                             // 源数据、宽度、高度和格式
        //     workflow->vicImgWidth_, workflow->vicImgHeight_, RK_FORMAT_BGRA_8888, dst_size, "WorkFlow::vicSinkCallback"); // 目标宽度、高度、格式和大小


        // 创建一个cv::Mat来包装整个YUV buffer
        // 因为NV12是Y平面和UV平面分开存储，所以需要特殊处理
        cv::Mat yuv_image_nv12(height * 3 / 2, width, CV_8UC1, mapinfo.data);

        // 将NV12格式转换为BGRA
        cv::Mat bgra_image;
        cv::cvtColor(yuv_image_nv12, bgra_image, cv::COLOR_YUV2BGRA_NV12);

        // 将图像调整为目标大小
        cv::Mat resized_image;
        cv::resize(bgra_image, resized_image, cv::Size(dst_width, dst_height), 0, 0, cv::INTER_LINEAR);

        // 创建一个指向调整大小后图像数据的指针
        unsigned char* vic_data = resized_image.data;


        if (vic_data) // 如果成功获取了调整大小的图像数据
        {
            // 创建OpenCV矩阵包装图像数据
            cv::Mat vic_mat(height, width, CV_8UC4, (void *)vic_data);
            cv::Mat undistorted_vic = vic_mat; // 未进行畸变校正的可见光图像





            // cv::Mat bgr_img;
            // cv::cvtColor(undistorted_vic, bgr_img, cv::COLOR_BGRA2BGR);


            // // 生成基于时间戳的唯一文件名
            // auto now = std::chrono::system_clock::now();
            // auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            //     now.time_since_epoch()).count();

            // std::string filename = "./debug_images/visible_image_" + 
            //                     std::to_string(timestamp) + ".jpg";
                                
            // cv::imwrite(filename, bgr_img);



            

            // 如果不需要红外图像进行检测，则使用可见光图像进行检测
            if (!workflow->irNeedForDetection)
            {
                // 以下是被注释掉的调试打印代码
                // std::cout << "=======================================" << std::endl;
                // std::cout << "     Visible light detection mode: " <<  std::endl;
                // std::cout << "=======================================" << std::endl;

                // 加锁更新检测图像
                std::lock_guard<std::mutex> lock(workflow->detection_image_mutex_);
                workflow->detection_image = undistorted_vic.clone();
            }

            // 如果需要进行可见光处理
            if (needProcessVic)
            {
                // 处理可见光+红外+激光模式(VIL)或可见光+红外模式(VI)下的图像融合
                if (workflow->m_blendType == BlendType::VIL || workflow->m_blendType == BlendType::VI)
                {
                    cv::Mat current_ir_image;
                    {
                        // 加锁获取当前的红外图像
                        std::lock_guard<std::mutex> lock(workflow->aligned_ir_for_fusion_mutex_);
                        current_ir_image = workflow->aligned_ir_for_fusion.clone();
                    }

                    // 如果红外图像有效，进行图像融合
                    if (!current_ir_image.empty())
                    {
                        RGAOperations::blendImages(undistorted_vic, current_ir_image);
                    }
                }

                // 加锁更新流输出图像
                std::lock_guard<std::mutex> lock(workflow->stream_image_mutex_);
                workflow->stream_image = undistorted_vic.clone();
            }

            // delete[] vic_data; // 释放通过RGA分配的内存
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Image conversion failed: " << e.what() << std::endl; // 记录图像转换失败的异常
    }

    // 清理GStreamer资源
    gst_buffer_unmap(buffer, &mapinfo); // 解除缓冲区映射
    gst_sample_unref(sample);           // 释放样本引用

    LOG_DEBUG("Visible light sink callback called"); // 记录回调函数调用
    return GST_FLOW_OK;                              // 返回成功状态
}

// 从GStreamer应用程序接收器拉取样本
GstSample *WorkFlow::pullSample(GstAppSink *appsink)
{
    return gst_app_sink_pull_sample(GST_APP_SINK(appsink)); // 使用GStreamer API拉取样本
}

// 映射GStreamer缓冲区
bool WorkFlow::mapBuffer(GstBuffer *buffer, GstMapInfo &mapinfo)
{
    return gst_buffer_map(buffer, &mapinfo, GST_MAP_READ); // 映射缓冲区为只读模式
}

// 从GStreamer应用程序接收器获取媒体属性（宽度和高度）
void WorkFlow::getCapsInfo(GstAppSink *appsink, int &width, int &height)
{
    GstCaps *caps = gst_pad_get_current_caps(GST_BASE_SINK_PAD(appsink)); // 获取当前caps
    GstStructure *structure = gst_caps_get_structure(caps, 0);            // 获取第一个结构
    gst_structure_get_int(structure, "width", &width);                    // 获取宽度
    gst_structure_get_int(structure, "height", &height);                  // 获取高度
    gst_caps_unref(caps);                                                 // 释放caps引用
}

// 创建视频编码管道
bool WorkFlow::createEncoderPipeline(int bpsRate)
{
    LOG_INFO("Creating encoder pipeline with BPS rate: " + std::to_string(bpsRate)); // 记录正在创建编码器管道

    Utils::bind_to_cpus({0});                                    // 绑定到CPU 0
    Logger::info("Thread-createEncoderPipeline bound to CPU 0"); // 记录线程绑定信息
    try
    {
        // 分配编码缓冲区内存
        g_encData = (unsigned char *)malloc(vicImgWidth_ * vicImgHeight_ * 3 / 2);
        if (!g_encData)
        {
            LOG_ERROR("Failed to allocate memory for encoding buffer"); // 记录内存分配失败
            throw std::runtime_error("Failed to allocate memory for encoding buffer");
        }
        memset(g_encData, 0, vicImgWidth_ * vicImgHeight_ * 3 / 2); // 初始化内存为0

        // 创建GStreamer管道和元素
        g_enc_pipeline = GST_PIPELINE(gst_pipeline_new(NULL));       // 创建管道
        g_enc_src = gst_element_factory_make("appsrc", NULL);        // 创建应用程序源
        g_enc_filter = gst_element_factory_make("capsfilter", NULL); // 创建格式过滤器
        g_encoder = gst_element_factory_make("mpph264enc", NULL);    // 创建H.264编码器
        g_parse = gst_element_factory_make("h264parse", NULL);       // 创建H.264解析器
        g_stream_sink = gst_element_factory_make("fakesink", NULL);  // 创建伪接收器

        // 检查元素是否创建成功
        if (!g_enc_pipeline || !g_enc_src || !g_enc_filter || !g_encoder || !g_parse || !g_stream_sink)
        {
            LOG_ERROR("Failed to create encoder pipeline elements"); // 记录元素创建失败
            throw std::runtime_error("Failed to create encoder pipeline elements");
        }

        // 以下是被注释掉的caps创建代码
        // GstCaps *caps = gst_caps_from_string(
        //     ("video/x-raw,width=" + std::to_string(vicImgWidth_) + ",height=" + std::to_string(vicImgHeight_) +
        //      ",framerate=25/1,format=NV12")
        //         .c_str());

        // 创建视频caps
        GstCaps *caps = gst_caps_from_string(
            ("video/x-raw,width=" + std::to_string(outputImgWidth_) + ",height=" + std::to_string(outputImgHeight_) +
             ",framerate=25/1,format=NV12")
                .c_str());
        g_object_set(G_OBJECT(g_enc_filter), "caps", caps, NULL); // 设置过滤器caps
        gst_caps_unref(caps);                                     // 释放caps引用

        // 设置编码器比特率
        setEncoderBps(bpsRate);

        // 在createEncoderPipeline函数中添加编码器高级设置
        if (g_encoder)
        {
            g_object_set(G_OBJECT(g_encoder),
                         "gop-size", 30, // 设置关键帧间隔
                         "gop-mode", 1,  // 固定GOP大小
                         "rc-mode", 1,   // 码率控制模式
                         "cabac", 1,     // 使用CABAC编码（Context-adaptive binary arithmetic coding）
                         "qp-min", 15,   // 最小量化参数
                         "qp-max", 35,   // 最大量化参数
                         NULL);
        }

        // 设置H.264解析器配置间隔
        g_object_set(G_OBJECT(g_parse), "config-interval", 1, NULL);

        // 设置流接收器信号处理
        g_object_set(G_OBJECT(g_stream_sink), "signal-handoffs", TRUE, NULL);
        g_signal_connect(G_OBJECT(g_stream_sink), "handoff", G_CALLBACK(mediaStreamProc), this); // 连接handoff信号
        g_object_set(G_OBJECT(g_stream_sink), "async", FALSE, NULL);

        // 将元素添加到管道中
        gst_bin_add_many(GST_BIN(g_enc_pipeline), g_enc_src, g_enc_filter, g_encoder, g_parse, g_stream_sink, NULL);

        // 链接管道元素
        if (!gst_element_link_many(g_enc_src, g_enc_filter, g_encoder, g_parse, g_stream_sink, NULL))
        {
            LOG_ERROR("Failed to link encoder pipeline elements"); // 记录链接元素失败
            throw std::runtime_error("Failed to link encoder pipeline elements");
        }

        // 设置管道为播放状态
        if (gst_element_set_state(GST_ELEMENT(g_enc_pipeline), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
        {
            LOG_ERROR("Failed to set encoder pipeline to playing state"); // 记录设置为播放状态失败
            throw std::runtime_error("Failed to set encoder pipeline to playing state");
        }

        LOG_INFO("Encoder pipeline created successfully"); // 记录编码器管道创建成功
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in createEncoderPipeline: " + std::string(e.what())); // 记录异常
        return false;
    }

    LOG_INFO("Encoder pipeline created successfully"); // 记录编码器管道创建成功
}

// 设置编码器比特率
void WorkFlow::setEncoderBps(int bpsRate)
{
    if (g_encoder) // 如果编码器已初始化
    {
        // 设置编码器参数
        g_object_set(G_OBJECT(g_encoder), "rc-mode", 1, "header-mode", 1,
                     "bps-min", bpsRate, "bps", bpsRate, "bps-max", bpsRate, // 设置最小、目标和最大比特率
                     "qp-init", 30, "qp-min", 30, NULL);                     // 设置初始和最小量化参数
        LOG_INFO("Encoder BPS set to " + std::to_string(bpsRate));           // 记录比特率设置
    }
    else
    {
        LOG_WARNING("Attempted to set encoder BPS, but encoder is not initialized"); // 记录编码器未初始化警告
    }
}


// 媒体流处理回调函数 - 处理编码后的H.264数据并通过网络发送
GstFlowReturn WorkFlow::mediaStreamProc(GstElement *sink, GstBuffer *buffer, GstPad *pad, gpointer data)
{
    WorkFlow *workflow = static_cast<WorkFlow *>(data); // 转换用户数据为WorkFlow对象指针
    GstMapInfo mapinfo;                                 // 用于映射GStreamer缓冲区的结构

    // 帧率计算变量
    static uint64_t frameCount = 0;                               // 帧计数器
    static auto lastPrintTime = std::chrono::steady_clock::now(); // 上次打印时间

    // 文件保存相关的静态变量（大部分被注释）
    static std::ofstream videoFile; // 视频文件输出流
    // static bool fileOpened = false;  // 文件已打开标志
    static std::string videoFileName; // 视频文件名
    // 添加写入缓冲区
    static std::vector<char> writeBuffer; // 写入缓冲区
    // const size_t BUFFER_SIZE = 1024 * 1024; // 1MB 缓冲区

    // 映射GStreamer缓冲区以便读取
    if (gst_buffer_map(buffer, &mapinfo, GST_MAP_READ))
    {
        // 数据有效性检查
        if (mapinfo.size <= 0)
        {
            LOG_ERROR("Invalid frame size detected"); // 记录无效帧大小错误
            gst_buffer_unmap(buffer, &mapinfo);       // 解除缓冲区映射
            return GST_FLOW_ERROR;                    // 返回错误状态
        }

        // 发送网络数据包 - 将编码后的H.264数据通过网络管理器发送
        workflow->networkManager_.sendPackage((char *)mapinfo.data, mapinfo.size);
        gst_buffer_unmap(buffer, &mapinfo); // 解除缓冲区映射

        // 增加帧计数
        ++frameCount;

        // 计算和打印帧率 - 每秒更新一次
        auto currentTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastPrintTime).count();
        if (duration >= 1)
        {
            // 计算帧率
            double fps = static_cast<double>(frameCount) / duration;
            
            // 打印帧率信息
            printf("Current frame rate: %.2f fps\n", fps);
            // LOG_INFO("Current frame rate: " + std::to_string(fps) + " fps");
            
            frameCount = 0;              // 重置帧计数器
            lastPrintTime = currentTime; // 更新上次打印时间
        }
    }
    else
    {
        LOG_ERROR("Failed to map h264 buffer"); // 记录映射缓冲区失败错误
        return GST_FLOW_ERROR;                  // 返回错误状态
    }

    return GST_FLOW_OK; // 返回成功状态
}

// 为点坐标添加随机扰动，模拟自然抖动效果
cv::Point add_perturbation(const cv::Point &point, int max_delta = 3)
{
    // 使用随机数生成器
    static std::random_device rd;                                      // 随机设备
    static std::mt19937 gen(rd());                                     // Mersenne Twister生成器
    static std::uniform_int_distribution<> dis(-max_delta, max_delta); // 均匀分布

    // 为x和y坐标分别添加随机扰动
    int new_x = point.x + dis(gen);
    int new_y = point.y + dis(gen);

    return cv::Point(new_x, new_y); // 返回添加扰动后的点
}



#include <opencv2/opencv.hpp>

// 编码与流式传输线程 - 处理图像并推送到编码器
void WorkFlow::encodingAndStreamingThread()
{
    LOG_INFO("Encoding and streaming thread started"); // 记录线程启动

    using clock = std::chrono::steady_clock;
    const auto frame_duration = std::chrono::milliseconds(40); // 25 fps = 40ms per frame
    bool draw_bounding_boxes_locally = config_.getDrawBoundingBoxesLocally();

    while (!shouldStop_)
    {
        updateThreadTimestamp(ThreadType::ENCODING_STREAMING);

        auto frame_start = clock::now();

        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lock(stream_image_mutex_);
            frame = stream_image.clone();
        }

        if (m_blendType != BlendType::VIL)
        {
            calcLidarManualSelection(distance_image, lidar_manual_selection_ctx);
            drawLidarManualSelection(frame, lidar_manual_selection_ctx);
        }

        if (!frame.empty())
        {
            bool is_valid = validateDisplayModeAndIP(display_mode, config_.getLocalIP());

            if (single_mode && !is_valid)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // =======================================================
            // 使用OpenCV替代RGAOperations::resizeImage
            // =======================================================
            
            // 声明一个Mat来保存最终的YUV图像数据
            cv::Mat yuv_image;

            try 
            {
                // 第一步：将BGRA格式的帧转换为YUV420（NV12）格式
                // OpenCV使用COLOR_BGRA2YUV_I420，但它输出的是I420，我们需要NV12
                // 这里我们分两步：先转成I420，再将I420转换成NV12
                // 更高效的方法是直接使用COLOR_BGRA2YUV_YV12，然后手动调整UV平面
                
                // 将BGRA转换为YUV420（I420，即YUV420p）
                cv::Mat temp_yuv;
                cv::cvtColor(frame, temp_yuv, cv::COLOR_BGRA2YUV_I420);

                // 检查是否需要调整分辨率
                if (frame.cols != outputImgWidth_ || frame.rows != outputImgHeight_)
                {
                    // 第二步：调整YUV图像的分辨率
                    cv::resize(temp_yuv, yuv_image, cv::Size(outputImgWidth_, outputImgHeight_ * 1.5));
                }
                else
                {
                    yuv_image = temp_yuv;
                }

                // 因为RGAOperations::resizeImage生成的是NV12 (YUV420SP)，
                // 而cv::COLOR_BGRA2YUV_I420生成的是I420 (YUV420P)。
                // 这里需要将I420（平面式）转换为NV12（半平面式）格式，以便与RK_FORMAT_YCbCr_420_SP兼容。
                // 我们可以使用一个辅助函数来完成这个转换，因为OpenCV没有内置的BGRA到NV12的直接转换。
                
                // 假设有一个函数 convertI420ToNV12(const cv::Mat& src, cv::Mat& dst)
                // convertI420ToNV12(yuv_image, yuv_image); 
                // 或者手动处理，这部分需要根据具体实现进行调整。
                // 简单的例子是创建一个新Mat并手动复制数据
                
                // 考虑到OpenCV的YUV格式支持，我们可以直接用 COLOR_BGRA2YUV_YV12，它的输出和I420非常相似
                // 只是UV平面的顺序不同，可以手动复制实现NV12
                
                // 为了简化，我们假设 `Utils::convertI420ToNV12` 是可用的
                // 或者在没有该函数的情况下，直接使用I420，并确保下游编码器可以处理它。
                // 这是一个潜在的兼容性问题，因为你的原始代码指定了RK_FORMAT_YCbCr_420_SP。
                
                // 以下是一个简单的I420到NV12的转换实现：
                // 这段代码可以封装在一个帮助函数中
                cv::Mat nv12_image(outputImgHeight_ * 1.5, outputImgWidth_, CV_8UC1);
                
                // 复制Y平面
                int y_size = outputImgWidth_ * outputImgHeight_;
                yuv_image.rowRange(0, outputImgHeight_).copyTo(nv12_image.rowRange(0, outputImgHeight_));
                
                // 复制并交错UV平面
                unsigned char* u_src = yuv_image.data + y_size;
                unsigned char* v_src = yuv_image.data + y_size * 5 / 4;
                unsigned char* uv_dst = nv12_image.data + y_size;

                for (int i = 0; i < y_size / 4; i++) {
                    uv_dst[i * 2] = u_src[i];
                    uv_dst[i * 2 + 1] = v_src[i];
                }
                
                yuv_image = nv12_image; // 最终的NV12图像
            }
            catch (const cv::Exception& e) 
            {
                LOG_ERROR("OpenCV error during image conversion or resize: " + std::string(e.what()));
                continue; // 跳过当前帧
            }
            
            unsigned char *mapinfo_data = yuv_image.data;

            if (mapinfo_data != nullptr)
            {
                if (draw_bounding_boxes_locally)
                {
                    for (int i = 0; i < detect_results.count; i++)
                    {
                        detect_result_t *det_result = &(detect_results.results[i]);

                        if (det_result->box.left < 0 || det_result->box.top < 0 ||
                            det_result->box.right > frame.cols || det_result->box.bottom > frame.rows)
                        {
                            std::cerr << "Warning: Invalid bounding box coordinates" << std::endl;
                            continue;
                        }

                        float scale_x = static_cast<float>(outputImgWidth_) / frame.cols;
                        float scale_y = static_cast<float>(outputImgHeight_) / frame.rows;

                        int scaled_x = static_cast<int>(det_result->box.left * scale_x);
                        int scaled_y = static_cast<int>(det_result->box.top * scale_y);
                        int scaled_width = static_cast<int>((det_result->box.right - det_result->box.left) * scale_x);
                        int scaled_height = static_cast<int>((det_result->box.bottom - det_result->box.top) * scale_y);

                        if (scaled_width <= 0 || scaled_height <= 0)
                        {
                            std::cerr << "Warning: Invalid scaled box dimensions" << std::endl;
                            continue;
                        }

                        if (scaled_x + scaled_width > outputImgWidth_ || scaled_y + scaled_height > outputImgHeight_)
                        {
                            scaled_width = std::min(scaled_width, outputImgWidth_ - scaled_x);
                            scaled_height = std::min(scaled_height, outputImgHeight_ - scaled_y);
                        }

                        std::cout << "Drawing scaled box: "
                                  << "original(x=" << det_result->box.left << ", y=" << det_result->box.top
                                  << ", w=" << (det_result->box.right - det_result->box.left)
                                  << ", h=" << (det_result->box.bottom - det_result->box.top) << ") "
                                  << "scaled(x=" << scaled_x << ", y=" << scaled_y
                                  << ", w=" << scaled_width << ", h=" << scaled_height << ")"
                                  << std::endl;

                        // 在YUV图像上绘制缩放后的矩形
                        Utils::draw_rectangle(mapinfo_data, outputImgWidth_, outputImgHeight_,
                                              scaled_x, scaled_y, scaled_width, scaled_height,
                                              235, 128, 128);
                    }
                }

                pushBufferToEncoder(mapinfo_data, outputImgWidth_, outputImgHeight_);
                // OpenCV的Mat会自动管理内存，所以不需要手动free
            }
            else
            {
                LOG_ERROR("Failed to process frame with OpenCV");
            }
        }
        else
        {
            // LOG_WARNING("Empty frame retrieved from streaming queue");
        }

        auto frame_end = clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);

        if (processing_time < frame_duration)
        {
            std::this_thread::sleep_for(frame_duration - processing_time);
        }
        else
        {
            LOG_WARNING("Frame processing took longer than 40ms: " + std::to_string(processing_time.count()) + "ms");
        }
    }
    LOG_INFO("Encoding and streaming thread ended");
}


// // 编码与流式传输线程 - 处理图像并推送到编码器
// void WorkFlow::encodingAndStreamingThread()
// {
//     LOG_INFO("Encoding and streaming thread started"); // 记录线程启动

//     using clock = std::chrono::steady_clock;                                  // 使用稳定时钟
//     const auto frame_duration = std::chrono::milliseconds(40);                // 25 fps = 40ms per frame
//     bool draw_bounding_boxes_locally = config_.getDrawBoundingBoxesLocally(); // 是否在本地绘制边界框

//     // 以下是被注释掉的变量定义
//     // cv::Point2f left_bottom(0, 0);  // 左下角点
//     // cv::Point2f right_bottom(0, 0); // 右下角点

//     // 主循环 - 持续处理图像直到停止信号
//     while (!shouldStop_)
//     {
//         // 更新线程时间戳
//         updateThreadTimestamp(ThreadType::ENCODING_STREAMING);

//         auto frame_start = clock::now(); // 记录帧处理开始时间

//         // 获取当前需要处理的图像
//         cv::Mat frame;
//         {
//             std::lock_guard<std::mutex> lock(stream_image_mutex_); // 加锁保护共享数据
//             frame = stream_image.clone();                          // 克隆图像以便安全处理
//         }


//         // cv::Mat frame(vicImgHeight_, vicImgWidth_, CV_8UC4); // 创建一个4通道图像


//         // 处理雷达手动选择区域 - 根据混合类型选择不同的处理方式
//         // calcLidarManualSelection(frame, lidar_manual_selection_ctx);
//         if (m_blendType != BlendType::VIL)
//         {
//             calcLidarManualSelection(distance_image, lidar_manual_selection_ctx); // 计算雷达选择区域
//             drawLidarManualSelection(frame, lidar_manual_selection_ctx);          // 在图像上绘制雷达选择区域
//         }

//         // 只处理非空图像
//         if (!frame.empty())
//         {
//             // 验证显示模式和IP配置是否匹配
//             bool is_valid = validateDisplayModeAndIP(display_mode, config_.getLocalIP());

//             // 以下是被注释掉的调试输出
//             // std::cout << "Display mode validation result: " << std::boolalpha << is_valid << std::endl;
//             // std::cout << "single_mode mode validation result: " << std::boolalpha << single_mode << std::endl;

//             // 如果是单一模式且验证失败，跳过处理
//             if (single_mode && !is_valid)
//             {
//                 // 休眠100毫秒以防止忙等待
//                 // std::cout << "rest 100 ms" << std::endl;
//                 std::this_thread::sleep_for(std::chrono::milliseconds(100));
//                 continue; // 跳过循环的剩余部分，开始下一次迭代
//             }

//             // 使用RGA硬件加速调整图像大小和格式 - 从BGRA转为YUV420
//             int dst_size = outputImgWidth_ * outputImgHeight_ * 1.5; // 计算目标大小 (YUV420需要宽*高*1.5的空间)
//             unsigned char *mapinfo_data = RGAOperations::resizeImage(
//                 frame.data, frame.cols, frame.rows, RK_FORMAT_BGRA_8888, // 源数据信息
//                 outputImgWidth_, outputImgHeight_,                       // 使用设置的输出分辨率
//                 RK_FORMAT_YCbCr_420_SP,                                  // 目标格式为YUV420
//                 dst_size, "rga_resize");                                               // 目标大小



//             // 如果成功获取了调整大小的图像数据
//             if (mapinfo_data != nullptr)
//             {

//                 // 在YUV图像上绘制对象检测的边界框
//                 if (draw_bounding_boxes_locally)
//                 {
//                     // 遍历所有检测结果
//                     for (int i = 0; i < detect_results.count; i++)
//                     {
//                         detect_result_t *det_result = &(detect_results.results[i]);

//                         // 检查边界框坐标是否有效
//                         if (det_result->box.left < 0 || det_result->box.top < 0 ||
//                             det_result->box.right > frame.cols || det_result->box.bottom > frame.rows)
//                         {
//                             std::cerr << "Warning: Invalid bounding box coordinates" << std::endl;
//                             continue; // 跳过无效坐标
//                         }

//                         // 计算从原始图像到输出图像的缩放因子
//                         float scale_x = static_cast<float>(outputImgWidth_) / frame.cols;
//                         float scale_y = static_cast<float>(outputImgHeight_) / frame.rows;

//                         // 缩放坐标以匹配输出分辨率
//                         int scaled_x = static_cast<int>(det_result->box.left * scale_x);
//                         int scaled_y = static_cast<int>(det_result->box.top * scale_y);
//                         int scaled_width = static_cast<int>((det_result->box.right - det_result->box.left) * scale_x);
//                         int scaled_height = static_cast<int>((det_result->box.bottom - det_result->box.top) * scale_y);

//                         // 验证缩放后的尺寸
//                         if (scaled_width <= 0 || scaled_height <= 0)
//                         {
//                             std::cerr << "Warning: Invalid scaled box dimensions" << std::endl;
//                             continue; // 跳过无效尺寸
//                         }

//                         // 确保不会超出调整大小后图像的边界
//                         if (scaled_x + scaled_width > outputImgWidth_ || scaled_y + scaled_height > outputImgHeight_)
//                         {
//                             scaled_width = std::min(scaled_width, outputImgWidth_ - scaled_x);
//                             scaled_height = std::min(scaled_height, outputImgHeight_ - scaled_y);
//                         }

//                         // 调试输出用于验证
//                         std::cout << "Drawing scaled box: "
//                                   << "original(x=" << det_result->box.left << ", y=" << det_result->box.top
//                                   << ", w=" << (det_result->box.right - det_result->box.left)
//                                   << ", h=" << (det_result->box.bottom - det_result->box.top) << ") "
//                                   << "scaled(x=" << scaled_x << ", y=" << scaled_y
//                                   << ", w=" << scaled_width << ", h=" << scaled_height << ")"
//                                   << std::endl;

//                         // 在YUV图像上绘制缩放后的矩形 (Y=235, U=128, V=128 对应白色)
//                         Utils::draw_rectangle(mapinfo_data, outputImgWidth_, outputImgHeight_,
//                                               scaled_x, scaled_y, scaled_width, scaled_height,
//                                               235, 128, 128);
//                     }
//                 }


//                 // 将处理后的数据推送到编码器
//                 pushBufferToEncoder(mapinfo_data, outputImgWidth_, outputImgHeight_);
//                 free(mapinfo_data); // 释放RGA分配的内存
//             }
//             else
//             {
//                 LOG_ERROR("Failed to resize frame"); // 记录调整图像大小失败
//             }
//         }
//         else
//         {
//             // 被注释掉的空帧警告
//             // LOG_WARNING("Empty frame retrieved from streaming queue");
//         }

//         // 计算处理这一帧所花费的时间
//         auto frame_end = clock::now();
//         auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);

//         // 如果处理时间少于40毫秒，则休眠剩余时间以维持恒定帧率
//         if (processing_time < frame_duration)
//         {
//             std::this_thread::sleep_for(frame_duration - processing_time);
//         }
//         else
//         {
//             // 如果处理时间超过40毫秒，记录警告
//             LOG_WARNING("Frame processing took longer than 40ms: " + std::to_string(processing_time.count()) + "ms");
//         }
//     }
//     LOG_INFO("Encoding and streaming thread ended"); // 记录线程结束
// }

// 将缓冲区数据推送到编码器
void WorkFlow::pushBufferToEncoder(const unsigned char *data, int width, int height)
{
    // 分配内存（如果尚未分配）
    if (g_encData == nullptr)
    {
        g_encData = (unsigned char *)malloc(width * height * 1.5); // 为YUV数据分配内存
        if (g_encData == nullptr)
        {
            LOG_ERROR("Failed to allocate memory for encoding buffer"); // 记录内存分配失败
            return;
        }
        LOG_DEBUG("Allocated memory for encoding buffer"); // 记录内存分配成功
    }

    // 复制数据到编码缓冲区
    memcpy(g_encData, data, width * height * 1.5); // 复制YUV数据

    // 创建GstBuffer并发送到编码器
    GstBuffer *buffer = gst_buffer_new_wrapped_full(
        (GstMemoryFlags)0, g_encData, width * height * 1.5, // 内存标志、数据和大小
        0, width * height * 1.5, g_encData, nullptr);       // 偏移量、大小、数据和销毁函数

    // 发送缓冲区到appsrc
    gboolean ret;
    g_signal_emit_by_name(g_enc_src, "push-buffer", buffer, &ret);

    // 检查发送结果（注释掉的调试代码）
    if (ret)
    {
        // LOG_DEBUG("Successfully pushed buffer to encoder");
    }
    else
    {
        // LOG_ERROR("Failed to push buffer to encoder");
    }

    // 释放GstBuffer引用
    gst_buffer_unref(buffer);
}

// 打印两点坐标 - 调试函数
void print_two_points(const two_points &points)
{
    std::cout << "Largest Y point (largesty1): ("
              << points.largesty1.x << ", "
              << points.largesty1.y << ")" << std::endl;

    std::cout << "Second largest Y point (largesty2): ("
              << points.largesty2.x << ", "
              << points.largesty2.y << ")" << std::endl;
}

// 限制值在指定范围内 - 辅助函数
float clamp(float value, float min, float max)
{
    if (value < min)
        return min; // 如果值小于最小值，返回最小值
    if (value > max)
        return max; // 如果值大于最大值，返回最大值
    return value;   // 否则返回原值
}
// 修正点的坐标，确保点在图像范围内并转换为整数值
void WorkFlow::fixPoints(two_points &points)
{
    // 限制x坐标在[0, vicImgWidth_-1]范围内
    points.largesty1.x = clamp(points.largesty1.x, 0, vicImgWidth_ - 1);
    points.largesty2.x = clamp(points.largesty2.x, 0, vicImgWidth_ - 1);

    // 限制y坐标在[0, vicImgHeight_-1]范围内
    points.largesty1.y = clamp(points.largesty1.y, 0, vicImgHeight_ - 1);
    points.largesty2.y = clamp(points.largesty2.y, 0, vicImgHeight_ - 1);

    // 转换为整数坐标（四舍五入）
    points.largesty1.x = round(points.largesty1.x);
    points.largesty1.y = round(points.largesty1.y);
    points.largesty2.x = round(points.largesty2.x);
    points.largesty2.y = round(points.largesty2.y);
}

// 计算两点之间的角度(与水平线的夹角)，返回0-180度的角度值
float calculateAngle(const cv::Point2f &p1, const cv::Point2f &p2)
{
    float dx = p2.x - p1.x; // 计算x方向差值
    float dy = p2.y - p1.y; // 计算y方向差值

    // 使用 atan2 计算角度（弧度），atan2考虑了四个象限
    float angle_rad = std::atan2(dy, dx);

    // 将弧度转换为角度
    float angle_deg = angle_rad * 180.0f / CV_PI;

    // 确保角度在 0-180 度之间
    if (angle_deg < 0)
    {
        angle_deg += 180.0f; // 负角度加180度转为正角度
    }

    return angle_deg; // 返回角度值
}

// 根据两个点计算矩形区域
cv::Rect calculateRectangle(const two_points &points)
{
    // 获取两个点的坐标
    float x1 = points.largesty1.x;
    float y1 = points.largesty1.y;
    float x2 = points.largesty2.x;
    float y2 = points.largesty2.y;

    // 计算左上角坐标(x,y)
    float x = std::min(x1, x2); // 取x坐标的较小值
    float y = std::max(y1, y2); // 取y坐标的较大值

    // 计算宽度和高度
    float width = std::abs(x1 - x2); // 宽度为x坐标差的绝对值
    float height = 60;               // 高度固定为60像素

    // 创建矩形
    cv::Rect rect(
        static_cast<int>(x),     // 左上角x坐标
        static_cast<int>(y),     // 左上角y坐标
        static_cast<int>(width), // 宽度
        static_cast<int>(height) // 高度
    );

    return rect; // 返回矩形
}

// 计算指定矩形区域内的平均距离值
float calculateAverageDistance(const cv::Mat &distance_image, const cv::Rect &boundingBox)
{
    // 确保边界框在图像范围内（使用&运算符取交集）
    cv::Rect safeBox = boundingBox & cv::Rect(0, 0, distance_image.cols, distance_image.rows);

    // 如果交集为空，表示边界框完全在图像外
    if (safeBox.empty())
    {
        return -1.0f; // 返回无效值表示边界框在图像外
    }

    float sum = 0.0f;    // 距离值总和
    int validPoints = 0; // 有效点数量

    // 遍历边界框内的所有像素
    for (int y = safeBox.y; y < safeBox.y + safeBox.height; ++y)
    {
        for (int x = safeBox.x; x < safeBox.x + safeBox.width; ++x)
        {
            float distance = distance_image.at<float>(y, x); // 获取距离值

            // 检查点是否有效（假设0或负值表示无效距离）
            if (distance > 0)
            {
                sum += distance; // 累加距离值
                validPoints++;   // 增加有效点计数
            }
        }
    }

    // 计算平均值
    if (validPoints > 0)
    {
        return sum / validPoints; // 返回平均距离
    }
    else
    {
        return -1.0f; // 如果没有有效点，返回无效值
    }
}

// 启动目标检测线程
void WorkFlow::startDetectionThread()
{
    detection_thread_running_ = true;                                          // 设置线程运行标志
    detection_thread_ = std::thread(&WorkFlow::detectionThreadFunction, this); // 创建检测线程
    LOG_INFO("Detection thread started");                                      // 记录线程启动
}

// 停止目标检测线程
void WorkFlow::stopDetectionThread()
{
    detection_thread_running_ = false; // 清除线程运行标志
    if (detection_thread_.joinable())  // 检查线程是否可加入
    {
        detection_thread_.join(); // 等待线程完成
    }
    LOG_INFO("Detection thread stopped"); // 记录线程停止
}


// 目标检测线程的主函数
void WorkFlow::detectionThreadFunction()
{
    LOG_INFO("Detection thread started");

    cv::Mat frame;
    detector_.initializeRKNN();

    // 线程主循环
    while (detection_thread_running_)
    {
        updateThreadTimestamp(ThreadType::DETECTION);

        // 如果当前是纯激光模式(L)，不执行检测
        if (m_blendType == BlendType::L)
        {
            // 重置 detect_results
            detect_results.id = 0;
            detect_results.count = 0;

            for (int i = 0; i < OBJ_NUMB_MAX_SIZE; ++i)
            {
                memset(detect_results.results[i].name, 0, sizeof(detect_results.results[i].name));
                detect_results.results[i].box.left = 0;
                detect_results.results[i].box.right = 0;
                detect_results.results[i].box.top = 0;
                detect_results.results[i].box.bottom = 0;
                detect_results.results[i].prop = 0.0f;
                detect_results.results[i].timestamp = std::chrono::steady_clock::time_point();
            }
            networkManager_.setDetectionResults(detect_results);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(detection_image_mutex_);
                frame = detection_image.clone();
            }
        }

        // 这里的冗余代码可以移除
        // {
        //     std::lock_guard<std::mutex> lock(detection_image_mutex_);
        //     frame = detection_image.clone();
        // }

        if (frame.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // =======================================================
        // 使用OpenCV替代RGAOperations::resizeImage
        // =======================================================
        cv::Mat resized_rgb_frame;

        try
        {
            // 确保源图像是BGRA格式，并将其转换为RGB
            cv::Mat temp_rgb;
            cv::cvtColor(frame, temp_rgb, cv::COLOR_BGRA2RGB);

            // 调整图像大小以匹配模型的输入尺寸
            cv::resize(temp_rgb, resized_rgb_frame, cv::Size(detector_.getModelWidth(), detector_.getModelHeight()));
        }
        catch (const cv::Exception& e)
        {
            LOG_ERROR("OpenCV error during image conversion or resize for detection: " + std::string(e.what()));
            continue; // 跳过当前帧
        }
        
        unsigned char *frame_data = resized_rgb_frame.data;

        if (frame_data != nullptr)
        {
            auto start_time = std::chrono::high_resolution_clock::now();

            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            auto timer = std::chrono::system_clock::to_time_t(now);
            std::stringstream time_str;
            time_str << std::put_time(std::localtime(&timer), "%Y-%m-%d %H:%M:%S")
                     << '.' << std::setfill('0') << std::setw(3) << ms.count();

            detect_results = detector_.detect(frame_data);
            networkManager_.setDetectionResults(detect_results);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            if (duration.count() < 40)
            {
                auto sleep_time = 40 - duration.count();
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
                end_time = std::chrono::high_resolution_clock::now();
                duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            }
            
            // OpenCV的Mat会自动管理内存，所以不需要手动 free(frame_data);
        }
    }

    LOG_INFO("Detection thread ended");
}


// // 目标检测线程的主函数
// void WorkFlow::detectionThreadFunction()
// {
//     LOG_INFO("Detection thread started"); // 记录线程开始执行

//     cv::Mat frame;              // 用于存储要处理的图像帧
//     detector_.initializeRKNN(); // 初始化神经网络检测器

//     int dst_size = detector_.getModelHeight() * detector_.getModelWidth() * 3; // 计算目标图像大小（RGB格式）

//     // 线程主循环
//     while (detection_thread_running_)
//     {
//         // 更新线程时间戳
//         updateThreadTimestamp(ThreadType::DETECTION);

//         // 如果当前是纯激光模式(L)，不执行检测
//         if (m_blendType == BlendType::L)
//         {
//             // 重置 detect_results
//             detect_results.id = 0;
//             detect_results.count = 0;

//             // 清空 results 数组中的每个 detect_result_t
//             for (int i = 0; i < OBJ_NUMB_MAX_SIZE; ++i)
//             {
//                 memset(detect_results.results[i].name, 0, sizeof(detect_results.results[i].name)); // 清空名称
//                 detect_results.results[i].box.left = 0;                                            // 重置边界框左边界
//                 detect_results.results[i].box.right = 0;                                           // 重置边界框右边界
//                 detect_results.results[i].box.top = 0;                                             // 重置边界框上边界
//                 detect_results.results[i].box.bottom = 0;                                          // 重置边界框下边界
//                 detect_results.results[i].prop = 0.0f;                                             // 重置置信度
//                 detect_results.results[i].timestamp = std::chrono::steady_clock::time_point();     // 重置时间戳
//             }
//             networkManager_.setDetectionResults(detect_results); // 发送空的检测结果

//             std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 暂停1秒
//             continue;                                                     // 继续下一次循环
//         }
//         else
//         {
//             // 获取当前的检测图像
//             {
//                 std::lock_guard<std::mutex> lock(detection_image_mutex_); // 加锁保护共享资源
//                 frame = detection_image.clone();                          // 克隆图像以便安全处理
//             }
//         }

//         // 再次获取当前的检测图像（可能是冗余代码）
//         {
//             std::lock_guard<std::mutex> lock(detection_image_mutex_); // 加锁保护共享资源
//             frame = detection_image.clone();                          // 克隆图像以便安全处理
//         }

//         // 如果图像为空，暂停一小段时间后继续
//         if (frame.empty())
//         {
//             std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 暂停10毫秒
//             continue;                                                   // 继续下一次循环
//         }

//         // 使用RGA硬件加速调整图像大小和格式（从BGRA转为RGB）
//         unsigned char *frame_data = RGAOperations::resizeImage(
//             frame.data, vicImgWidth_, vicImgHeight_, RK_FORMAT_BGRA_8888,                        // 源数据信息
//             detector_.getModelWidth(), detector_.getModelHeight(), RK_FORMAT_RGB_888, dst_size, "detection"); // 目标信息

//         // 以下是被注释掉的简化版检测代码
//         // if (frame_data != nullptr)
//         // {
//         //     // Perform detection
//         //     detect_results = detector_.detect(frame_data);
//         //     networkManager_.setDetectionResults(detect_results);
//         //
//         //     // Free the resized image buffer
//         //     free(frame_data);
//         // }

//         // 如果成功获取了调整大小的图像数据
//         if (frame_data != nullptr)
//         {
//             // 获取开始时间点，用于计算检测耗时
//             auto start_time = std::chrono::high_resolution_clock::now();

//             // 获取当前时间戳（精确到毫秒），用于日志记录
//             auto now = std::chrono::system_clock::now();
//             auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
//             auto timer = std::chrono::system_clock::to_time_t(now);
//             std::stringstream time_str;
//             time_str << std::put_time(std::localtime(&timer), "%Y-%m-%d %H:%M:%S")
//                      << '.' << std::setfill('0') << std::setw(3) << ms.count();

//             // 执行检测
//             detect_results = detector_.detect(frame_data);       // 调用检测器执行目标检测
//             networkManager_.setDetectionResults(detect_results); // 发送检测结果到网络管理器

//             // 计算检测耗时
//             auto end_time = std::chrono::high_resolution_clock::now();
//             auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

//             // 以下是被注释掉的打印时间信息代码
//             // // 打印时间信息
//             // printf("Detection time: %ld ms, Timestamp: %s\n",
//             //     duration.count(), time_str.str().c_str());

//             // 如果检测耗时小于40ms，则补充睡眠以保持稳定的25fps帧率
//             if (duration.count() < 40)
//             {
//                 auto sleep_time = 40 - duration.count();                            // 计算需要睡眠的时间
//                 std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time)); // 睡眠补充时间
//                 // 更新实际总耗时
//                 end_time = std::chrono::high_resolution_clock::now();
//                 duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
//             }

//             // 以下是被注释掉的打印更新后的时间信息代码
//             // // 打印时间信息
//             // printf("Detection time: %ld ms, Timestamp: %s\n",
//             //     duration.count(), time_str.str().c_str());

//             // 释放RGA分配的图像缓冲区
//             free(frame_data);
//         }

//         // 以下是被注释掉的睡眠代码
//         // // Sleep for a short time to avoid busy-waiting
//         // std::this_thread::sleep_for(std::chrono::milliseconds(40));
//     }

//     LOG_INFO("Detection thread ended"); // 记录线程结束
// }


// 停止激光雷达线程
void WorkFlow::stopLidarThread()
{
    lidar_thread_running_ = false; // 清除线程运行标志
    if (lidar_thread_.joinable())  // 检查线程是否可加入
    {
        lidar_thread_.join(); // 等待线程完成
    }
    LOG_INFO("Lidar thread stopped"); // 记录线程停止
}

// 激光雷达数据处理线程的主函数
void WorkFlow::lidarThreadFunction()
{
    LOG_INFO("Lidar thread started"); // 记录线程开始执行
    processLidarData();               // 处理激光雷达数据
    LOG_INFO("Lidar thread ended");   // 记录线程结束
}

// 激光雷达数据存储相关的全局变量
std::string output_directory = "lidar_data"; // 输出文件夹路径
const int MAX_FILES = 1000;                  // 最大文件数限制
std::vector<std::string> existing_files;     // 用于追踪已存在的文件

// 检查文件或目录是否存在
bool exists(const std::string &path)
{
    struct stat buffer;                        // 文件状态缓冲区
    return (stat(path.c_str(), &buffer) == 0); // 返回stat调用的结果（0表示成功，即路径存在）
}

// 处理激光雷达数据的主函数
void WorkFlow::processLidarData()
{
    LOG_INFO("Processing Lidar data"); // 记录开始处理激光雷达数据

    // 从配置中获取激光雷达相关参数
    int lidarPortDat = config_.getLidarPortDat(); // 获取数据端口
    int lidarPortDev = config_.getLidarPortDev(); // 获取设备端口
    std::string localIP = config_.getLocalIP();   // 获取本地IP地址
    std::string lidarIP = config_.getLidarIP();   // 获取激光雷达IP地址

    // 设置激光雷达的端口和IP
    m_GetLidarData->setPortAndIP(lidarPortDat, lidarPortDev, localIP, lidarIP);
    m_GetLidarData->LidarStart(); // 启动激光雷达数据采集

    // 获取激光雷达内参和外参
    Config::LidarIntrinsic intrinsic = config_.getLidarIntrinsic(); // 获取内参矩阵
    extrinsicMatrix = config_.getLidarExtrinsic();                  // 获取外参矩阵
    fx = intrinsic.fx;                                              // 焦距x分量
    fy = intrinsic.fy;                                              // 焦距y分量
    cx = intrinsic.cx;                                              // 光心x坐标
    cy = intrinsic.cy;                                              // 光心y坐标

    std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 等待2秒，确保激光雷达正常启动

    m_LidarData.reserve(100000); // 预留足够的容量，根据实际情况调整

    // 主循环 - 持续处理激光雷达数据直到线程停止信号
    while (lidar_thread_running_)
    {
        // 更新线程时间戳
        updateThreadTimestamp(ThreadType::LIDAR);

        // 检查激光雷达参数状态
        if (!m_GetLidarData->getLidarParamState(mLidarStateParam, mInfo1))
        {
            lidar_status = 0x01;                               // 设置错误状态
            networkManager_.setLidarStatusState(lidar_status); // 更新网络管理器中的状态
        }
        else
        {
            lidar_status = 0x00;                               // 设置正常状态
            networkManager_.setLidarStatusState(lidar_status); // 更新网络管理器中的状态
        }

        // 检查是否有新帧可用
        if (m_GetLidarData->isFrameOK)
        {
            // 尝试获取激光雷达帧数据
            std::shared_ptr<std::vector<MuchLidarData>> m_LidarData_temp;
            std::string mInfo;
            if (!m_GetLidarData->getLidarPerFrameDate(m_LidarData_temp, mInfo))
            {
                // 获取失败的处理被注释掉了
                // std::cout << mInfo << std::endl;
                // std::this_thread::sleep_for(std::chrono::milliseconds(500));
                // continue;;
            }

            // 以下是被注释掉的调试输出代码
            // for (size_t i = 0; i < m_LidarData_temp->size(); ++i)
            // {
            //     std::cout << "Number of points in m_LidarData: " << m_LidarData.size() << std::endl;
            //     std::cout << "Number of points in m_LidarData_temp: " << m_LidarData_temp->size() << std::endl;
            //     std::cout.width(20);
            //     std::cout << "Mtimestamp_nsce    =    " << (*m_LidarData_temp)[i].Mtimestamp_nsce << std::endl;
            // }

            // if (!m_LidarData_temp->empty())
            // {
            //     // Mtimestamp_nsce的类型为uint64_t，以纳秒为单位
            //     std::cout.width(20);
            //     // std::cout << "Mtimestamp_nsce    =    " << m_LidarData_temp->back().Mtimestamp_nsce << std::endl;
            // }

            // 将获取到的点云数据复制到 m_LidarData 中
            m_LidarData.clear();             // 清空之前的数据
            m_LidarData = *m_LidarData_temp; // 复制新数据



            // // ===== 添加保存原始点云数据 =====
            // if (!m_LidarData.empty()) {
            //     static int frame_count = 0;
            //     frame_count++;
                
            //     // 每10帧保存一次（可根据需要调整）
            //     if (frame_count % 10 == 0) {
            //         std::string filename = "./pointclouds/pointcloud_" + std::to_string(frame_count) + ".csv";
            //         savePointCloudCSV(m_LidarData, filename);
            //     }
            // }
            // // ===== 保存代码结束 =====



            // 初始化距离图 - 创建与视频图像相同尺寸的浮点型矩阵，用于存储距离值
            cv::Mat distance_mat = cv::Mat::zeros(vicImgHeight_, vicImgWidth_, CV_32FC1);

            // 对点云数据进行配准处理 - 将3D点云投影到2D图像平面
            for (const auto &point : m_LidarData)
            {
                double ix = point.X; // 点云X坐标
                double iy = point.Y; // 点云Y坐标
                double iz = point.Z; // 点云Z坐标

                // 使用外参矩阵变换点云坐标到相机坐标系
                double x = extrinsicMatrix.at<double>(0, 0) * ix + extrinsicMatrix.at<double>(0, 1) * iy +
                           extrinsicMatrix.at<double>(0, 2) * iz + extrinsicMatrix.at<double>(0, 3);
                double y = extrinsicMatrix.at<double>(1, 0) * ix + extrinsicMatrix.at<double>(1, 1) * iy +
                           extrinsicMatrix.at<double>(1, 2) * iz + extrinsicMatrix.at<double>(1, 3);
                double z = extrinsicMatrix.at<double>(2, 0) * ix + extrinsicMatrix.at<double>(2, 1) * iy +
                           extrinsicMatrix.at<double>(2, 2) * iz + extrinsicMatrix.at<double>(2, 3);

                if (z <= 0) // 如果Z坐标非正，跳过该点（在相机后方）
                    continue;

                // 使用针孔相机模型将3D点投影到图像平面
                int x1 = static_cast<int>((x / z) * fx + cx);
                int y1 = static_cast<int>((y / z) * fy + cy);

                // 检查投影点是否在图像范围内
                if (x1 >= 0 && y1 >= 0 && x1 < vicImgWidth_ && y1 < vicImgHeight_)
                {
                    // 计算3D点到原点的距离
                    double distance = std::sqrt(ix * ix + iy * iy + iz * iz);
                    // 更新距离图，如果当前距离大于已存储的距离则更新
                    distance_mat.at<float>(y1, x1) = std::max(static_cast<float>(distance),
                                                              distance_mat.at<float>(y1, x1));
                }
            }

            // 以下是被注释掉的计算选择区域代码
            // calcLidarManualSelection(distance_image, lidar_manual_selection_ctx);

            // 以下是被注释掉的互斥锁代码
            // {
            //     std::lock_guard<std::mutex> lock(distance_image_for_fusion_mutex_);
            distance_image = distance_mat.clone(); // 更新距离图像
            // }

            // 根据不同的混合类型处理激光雷达数据
            if (m_blendType == BlendType::VL || m_blendType == BlendType::IL ||
                m_blendType == BlendType::VIL) // 需要与其他图像融合的模式
            {
                {
                    std::lock_guard<std::mutex> lock(aligned_lidar_for_fusion_mutex_); // 加锁保护共享资源
                    networkManager_.setAlignedLidarForFusion(distance_mat.clone());    // 设置用于融合的对齐激光雷达数据
                }
            }
            else if (m_blendType == BlendType::L) // 纯激光雷达模式
            {
                // 添加伪彩色变换，将距离图可视化
                cv::Mat distance_img_bgr = cv::Mat(vicImgHeight_, vicImgWidth_, CV_8UC4);        // 创建BGRA图像
                cv::Mat distance_img_uch = cv::Mat::zeros(vicImgHeight_, vicImgWidth_, CV_8UC1); // 创建单通道图像

                // 将浮点型距离图转换为8位无符号整型，缩放因子为255/20（假设最大距离为20米）
                distance_mat.convertTo(distance_img_uch, CV_8UC1, 255.0 / 20.0);

                // 创建膨胀操作的结构元素，用于增强点云的可视效果
                int dilation_size = 5;
                cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                            cv::Size(2 * dilation_size + 1, 2 * dilation_size + 1),
                                                            cv::Point(dilation_size, dilation_size));
                cv::dilate(distance_img_uch, distance_img_uch, element); // 对图像进行膨胀操作

                // 应用伪彩色变换（使用OpenCV并行处理以提高性能）
                cv::parallel_for_(cv::Range(0, distance_img_uch.rows), [&](const cv::Range &range)
                                  {
                    for (int y = range.start; y < range.end; ++y)
                    {
                        const uint8_t* src = distance_img_uch.ptr<uint8_t>(y);  // 源图像行指针
                        cv::Vec4b* dst = distance_img_bgr.ptr<cv::Vec4b>(y);    // 目标图像行指针
                        for (int x = 0; x < distance_img_uch.cols; ++x)
                        {
                            uint8_t u = src[x];  // 获取距离值
                            cv::Vec3b color = u > 0 ? dist2ColorMap[u] : cv::Vec3b(0, 0, 0);  // 查表获取伪彩色
                            dst[x] = cv::Vec4b(color[0], color[1], color[2], 255);  // 设置完全不透明
                        }
                    } });

                // 更新共享的伪彩色雷达图像
                {
                    std::lock_guard<std::mutex> lock(stream_image_mutex_); // 加锁保护共享资源
                    stream_image = distance_img_bgr.clone();               // 更新流图像
                }
            }
            else // 其他模式（不使用激光雷达数据）
            {
                cv::Mat distance_mat = cv::Mat::zeros(vicImgHeight_, vicImgWidth_, CV_32FC1); // 创建空距离图

                // 清空距离图
                {
                    std::lock_guard<std::mutex> lock(stream_image_mutex_);          // 加锁保护共享资源
                    networkManager_.setAlignedLidarForFusion(distance_mat.clone()); // 设置空的对齐激光雷达数据
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(40)); // 休眠40毫秒（约25fps）
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(40)); // 休眠40毫秒（约25fps）
            m_LidarData_temp->clear();                                  // 清空临时点云数据
        }
        else // 没有新帧时的处理
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 短暂休眠
        }

        // 被注释掉的释放代码
        // distance_mat.release();
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 休眠100毫秒
        LOG_DEBUG("Processed Lidar data");                           // 记录处理完成
    }

    m_GetLidarData->LidarStop(); // 停止激光雷达数据采集
    m_LidarData.clear();         // 清空点云数据
    m_LidarData.shrink_to_fit(); // 释放多余内存

    LOG_INFO("Lidar data processing completed"); // 记录处理完成
}



// 计算激光雷达手动选区的距离信息
void WorkFlow::calcLidarManualSelection(cv::Mat &mat_distance, LidarManualSelectionContext &selection)
{
    // 以下是被注释掉的调试输出代码
    // // 使用方法2打印
    // printLidarMode(selection.ctrl.mode);

    // 只处理激活状态的选区
    if (selection.ctrl.mode == LidarManualSelectionMode::ACTIVATE)
    {
        selection.empty = true; // 初始设置为空
        int roi_len = 32;       // 默认ROI尺寸

        // 根据选区大小设置具体尺寸
        switch (selection.ctrl.size)
        {
        case LidarManualSelectionSize::S16:
            roi_len = 16; // 小尺寸
            break;
        case LidarManualSelectionSize::S32:
            roi_len = 32; // 中尺寸
            break;
        case LidarManualSelectionSize::S64:
            roi_len = 64; // 大尺寸
            break;
        }
        int roi_len_half = roi_len >> 1; // 计算半尺寸（除以2）

        // 计算ROI边界，确保在图像范围内
        int lf = std::max(0, std::min(selection.ctrl.x - roi_len_half, mat_distance.cols)); // 左边界
        int tp = std::max(0, std::min(selection.ctrl.y - roi_len_half, mat_distance.rows)); // 上边界
        int rt = std::max(0, std::min(lf + roi_len, mat_distance.cols));                    // 右边界
        int bt = std::max(0, std::min(tp + roi_len, mat_distance.rows));                    // 下边界

        // 创建矩形区域
        selection.roi.rect = cv::Rect(lf, tp, rt - lf, bt - tp);

        uint64_t num = 0;          // 有效点数计数器
        if (!mat_distance.empty()) // 确保距离图不为空
        {
            cv::Mat mat_roi = mat_distance(selection.roi.rect); // 提取ROI区域
            double sum_distance = 0.0;                          // 距离总和

            // 遍历ROI中的所有像素
            for (int i = 0; i < mat_roi.rows; ++i)
            {
                const float *row_ptr = mat_roi.ptr<float>(i); // 获取行指针
                for (int j = 0; j < mat_roi.cols; ++j)
                {
                    if (row_ptr[j] > 0) // 如果像素值大于0（有效距离）
                    {
                        sum_distance += row_ptr[j]; // 累加距离
                        ++num;                      // 增加有效点计数
                    }
                }
            }
            // 计算平均距离
            selection.roi.ave_distance = (num > 0) ? sum_distance / num : 0.0;

            // 以下是被注释掉的日志记录代码
            // // 写入日志文件
            // std::ofstream log_file;
            // log_file.open("lidar_distance.log", std::ios::app); // 以追加模式打开文件
            // if (log_file.is_open())
            // {
            //     // 获取当前时间
            //     auto now = std::chrono::system_clock::now();
            //     auto now_time = std::chrono::system_clock::to_time_t(now);
            //     // log_file << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S")
            //     //         << " Average Distance: " << selection.roi.ave_distance
            //     //         << "m Points Count: " << num << std::endl;
            //     log_file << selection.roi.ave_distance  << " " << std::endl;
            //     log_file.close();
            // }
        }

        // 计算俯仰角和方位角
        selection.roi.pitch = (tp + bt) * 0.5 * 58.8 / vicImgHeight_ - 29.4;   // 俯仰角计算
        selection.roi.azimuth = (lf + rt) * 0.5 * 104.6 / vicImgWidth_ - 52.3; // 方位角计算

        // 格式化显示文本
        std::stringstream ss;
        if (num > 0)
        {
            ss << "Distance: " << selection.roi.ave_distance << "m, "; // 有效距离
        }
        else
        {
            ss << "Distance: Invalid, "; // 无效距离
        }
        ss << "Pitch: " << std::fixed << std::setprecision(1) << selection.roi.pitch << " deg, "    // 俯仰角
           << "Azimuth: " << std::fixed << std::setprecision(1) << selection.roi.azimuth << " deg"; // 方位角
        selection.formatted_txt.label = ss.str();                                                   // 设置标签文本

        // 计算文本大小，用于后续绘制
        selection.formatted_txt.size = cv::getTextSize(
            selection.formatted_txt.label,
            selection.formatted_txt.style.face,
            selection.formatted_txt.style.scale,
            selection.formatted_txt.style.thickness,
            &selection.formatted_txt.baseline);
        selection.empty = false; // 标记选区不为空
    }
}




// 实现线程监控相关函数
void WorkFlow::registerThreadMonitor(
    ThreadType type, 
    const std::string& name,
    uint64_t timeout_seconds,
    std::function<bool()> reinitialize_function)
{
    std::lock_guard<std::mutex> lock(thread_monitors_mutex_);
    
    auto info = std::make_shared<ThreadMonitorInfo>();
    info->callback_counter = 0;
    info->atomic_counter.store(0);
    info->last_timestamp.store(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count());
    info->timeout_seconds = timeout_seconds;
    info->reinitialize_function = reinitialize_function;
    info->thread_name = name;
    
    thread_monitors[type] = info;
    
    LOG_INFO("Registered thread monitor for: " + name + 
            " with timeout: " + std::to_string(timeout_seconds) + " seconds");
}

void WorkFlow::updateThreadTimestamp(ThreadType type)
{
    std::lock_guard<std::mutex> lock(thread_monitors_mutex_);
    
    auto it = thread_monitors.find(type);
    if (it != thread_monitors.end())
    {
        it->second->atomic_counter.fetch_add(1);
        it->second->last_timestamp.store(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count());
    }
}

void WorkFlow::startMonitorThread()
{
    if (!monitor_thread_running_)
    {
        monitor_thread_running_ = true;
        monitor_thread_ = std::thread(&WorkFlow::monitorThreadFunction, this);
        LOG_INFO("Thread monitor started");
    }
}

void WorkFlow::stopMonitorThread()
{
    if (monitor_thread_running_)
    {
        monitor_thread_running_ = false;
        if (monitor_thread_.joinable())
        {
            monitor_thread_.join();
        }
        LOG_INFO("Thread monitor stopped");
    }
}

void WorkFlow::monitorThreadFunction()
{
    LOG_INFO("Thread monitor running");
    Utils::bind_to_cpus({3}); // 绑定到CPU 3
    
    // 记录上次检查的计数器值
    std::map<ThreadType, uint64_t> last_checked_counters;
    
    // 监控循环
    while (monitor_thread_running_ && !shouldStop_)
    {
        // 等待检查间隔
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 当前时间
        uint64_t current_time = 
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        
        // 检查所有线程
        std::lock_guard<std::mutex> lock(thread_monitors_mutex_);
        
        for (auto& monitor_pair : thread_monitors)
        {
            ThreadType type = monitor_pair.first;
            auto& info = monitor_pair.second;
            
            // 获取当前计数器值
            uint64_t current_counter = info->atomic_counter.load();
            
            // 更新非原子计数器，用于比较
            info->callback_counter = current_counter;
            
            // 如果是第一次检查，初始化last_checked_counters
            if (last_checked_counters.find(type) == last_checked_counters.end())
            {
                last_checked_counters[type] = current_counter;
                continue;
            }
            
            // 计算自上次活动的时间差
            uint64_t time_since_last_activity = current_time - info->last_timestamp.load();
            
            // 如果计数器未更新且已超时
            if (current_counter == last_checked_counters[type] && 
                time_since_last_activity > info->timeout_seconds)
            {
                LOG_WARNING(info->thread_name + " timeout detected! "
                           "Counter: " + std::to_string(current_counter) + 
                           ", Time since last activity: " + 
                           std::to_string(time_since_last_activity) + " seconds");
                
                // 尝试重新初始化
                LOG_INFO("Attempting to reinitialize " + info->thread_name + "...");
                
                bool result = false;
                try {
                    result = info->reinitialize_function();
                }
                catch (const std::exception& e) {
                    LOG_ERROR("Exception during " + info->thread_name + 
                             " reinitialization: " + std::string(e.what()));
                }
                
                if (result)
                {
                    LOG_INFO(info->thread_name + " reinitialized successfully");
                }
                else
                {
                    LOG_ERROR("Failed to reinitialize " + info->thread_name);
                }
                
                // 重置时间戳，避免连续重试
                info->last_timestamp.store(current_time);
            }
            
            // 更新上次检查的计数器值
            last_checked_counters[type] = current_counter;
        }
    }
    
    LOG_INFO("Thread monitor ended");
}

// 实现各个线程的重新初始化函数
bool WorkFlow::reinitializeVisibleSource()
{
    LOG_INFO("Reinitializing visible light source");
    
    try {
        // 停止当前的视频管道
        if (g_pipeline)
        {
            gst_element_set_state(GST_ELEMENT(g_pipeline), GST_STATE_NULL);
            gst_object_unref(g_pipeline);
            g_pipeline = nullptr;
            g_src = nullptr;
            g_video_filter = nullptr;
            g_video_sink = nullptr;
        }
        
        // 等待资源释放
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 重新初始化可见光源
        std::string visibleDevice = config_.getVisibleVideoDevice();
        GstAppSinkCallbacks vic_callbacks = {NULL, NULL, vicSinkCallback, NULL};
        
        bool result = createVisibleVideoSource(vicImgWidth_, vicImgHeight_, 
                                              visibleDevice.c_str(), vic_callbacks);
        
        return result;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in reinitializeVisibleSource: " + std::string(e.what()));
        return false;
    }
}

bool WorkFlow::reinitializeIRSource()
{
    LOG_INFO("Reinitializing infrared source");
    
    try {
        // 停止当前的视频管道
        if (g_pipeline)
        {
            gst_element_set_state(GST_ELEMENT(g_pipeline), GST_STATE_NULL);
            gst_object_unref(g_pipeline);
            g_pipeline = nullptr;
            g_src = nullptr;
            g_video_filter = nullptr;
            g_video_sink = nullptr;
        }
        
        // 等待资源释放
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 重新初始化红外源
        std::string irDevice = config_.getIRVideoDevice();
        bool result = initializeIRSource(irDevice.c_str());
        
        return result;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in reinitializeIRSource: " + std::string(e.what()));
        return false;
    }
}

bool WorkFlow::reinitializeEncodingStreaming()
{
    LOG_INFO("Reinitializing encoding and streaming");
    
    try {
        // 暂时标记应该停止，以便让编码线程可以结束
        bool original_stop_flag = shouldStop_;
        shouldStop_ = true;
        
        // 等待编码线程结束
        if (encoding_thread_.joinable())
        {
            encoding_thread_.join();
        }
        
        // 恢复原来的停止标志
        shouldStop_ = original_stop_flag;
        
        // 重新创建编码管道
        int bpsRate = config_.getInitialBpsRate();
        if (!createEncoderPipeline(bpsRate))
        {
            LOG_ERROR("Failed to recreate encoder pipeline");
            return false;
        }
        
        // 启动新的编码和推流线程
        encoding_thread_ = std::thread(&WorkFlow::encodingAndStreamingThread, this);
        LOG_INFO("Encoding and streaming thread restarted");
        
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in reinitializeEncodingStreaming: " + std::string(e.what()));
        return false;
    }
}

bool WorkFlow::reinitializeDetection()
{
    LOG_INFO("Reinitializing detection");
    
    try {
        // 停止当前的检测线程
        stopDetectionThread();
        
        // 启动新的检测线程
        startDetectionThread();
        
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in reinitializeDetection: " + std::string(e.what()));
        return false;
    }
}

bool WorkFlow::reinitializeLidar()
{
    LOG_INFO("Reinitializing lidar");
    
    try {
        // 停止当前的激光雷达线程
        stopLidarThread();
        
        // 重新启动激光雷达线程
        lidar_thread_running_ = true;
        lidar_thread_ = std::thread(&WorkFlow::lidarThreadFunction, this);
        LOG_INFO("Lidar thread restarted");
        
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in reinitializeLidar: " + std::string(e.what()));
        return false;
    }
}

bool WorkFlow::reinitializeNetworkControl()
{
    LOG_INFO("Reinitializing network control");
    
    try {
        // 由于NetworkManager没有stopReceivingControl方法，我们只能重新启动
        networkManager_.startReceivingControl([this](const uint8_t *frame, size_t length)
                                              { this->processFormat(frame, length); });
        
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in reinitializeNetworkControl: " + std::string(e.what()));
        return false;
    }
}

bool WorkFlow::reinitializeNetworkStatus()
{
    LOG_INFO("Reinitializing network status");
    
    try {
        // 由于NetworkManager没有stopSendingStatus方法，我们只能重新启动
        networkManager_.startSendingStatus();
        networkManager_.startPrepareStatusFrameThread();
        
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Exception in reinitializeNetworkStatus: " + std::string(e.what()));
        return false;
    }
}


// 实现CSV保存函数
void WorkFlow::savePointCloudCSV(const std::vector<MuchLidarData>& lidar_data, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return; // 保存失败则直接返回
    }
    
    // // 写入CSV头部
    // file << "X,Y,Z,ID,H_angle,V_angle,Distance,Intensity,Timestamp_ns\n";
    
    // // 写入点云数据
    // for (const auto& point : lidar_data) {
    //     file << point.X << "," << point.Y << "," << point.Z << "," 
    //          << point.ID << "," << point.H_angle << "," << point.V_angle << ","
    //          << point.Distance << "," << point.Intensity << "," << point.Mtimestamp_nsce << "\n";
    // }

    // 写入CSV头部
    file << "X,Y,Z,Intensity\n";
    
    // 写入点云数据
    for (const auto& point : lidar_data) {
        file << point.X << "," << point.Y << "," << point.Z << "," << point.Intensity << "\n";
    }

    file.close();
}