/**
 * NetworkManager.cpp - 网络通信与设备控制管理模块
 *
 * 本文件实现了系统的网络通信和设备控制管理功能。
 * 主要功能包括：
 * 1. UDP通信 - 实现状态帧发送和控制帧接收的网络通信
 * 2. 串口控制 - 管理可见光相机和红外相机的串口通信和控制
 * 3. 状态管理 - 监控和更新系统各组件的工作状态和错误状态
 * 4. 目标处理 - 处理和分析检测到的目标信息（位置、距离、尺寸等）
 * 5. 数据分发 - 通过RTSP和UDP实现数据流的传输和分发
 * 6. 多线程架构 - 使用独立线程处理不同任务，提高系统响应性
 * 7. 调试支持 - 提供丰富的日志记录和状态监控功能
 * 8. 距离分析 - 结合激光雷达数据计算目标距离和碰撞预警
 *
 * 该模块负责整个系统的网络通信和信息交互，是系统运行的核心组件。
 */

#include "NetworkManager.h"
#include <iostream>
#include <cstring>      // 提供内存操作函数如 memset 等
#include <thread>       // 提供线程支持
#include <sys/socket.h> // 套接字编程相关
#include <netinet/in.h> // 提供网络地址结构体
#include <arpa/inet.h>  // 提供 IP 地址转换功能
#include <unistd.h>     // 提供 POSIX API 如 close()

// NetworkManager 构造函数，初始化各种状态变量
NetworkManager::NetworkManager(const Config &config)
    : config_(config), shouldStop_(false)
{
    // 初始化状态帧的各种变量
    boot_time_ = 0;                                    // 启动时间
    blend_status_ = 0;                                 // 混合状态
    work_status_ = 0;                                  // 工作状态
    lidar_status_ = 0;                                 // 激光雷达状态
    ir_error_status_ = 0;                              // 红外错误状态
    ir_device_status_ = 0;                             // 红外设备状态
    vic_error_status_ = 0;                             // 可见光错误状态
    vic_device_status_ = 0;                            // 可见光设备状态
    collision_warning_ = 0;                            // 碰撞警告
    preset_angle_ = 0;                                 // 预设角度
    shore_distance_ = 0;                               // 岸距离
    target_num_ = 0;                                   // 目标数量
    memset(g_status_frame, 0, sizeof(g_status_frame)); // 清零状态帧缓冲区

    // 从配置获取 PJ IP 地址
    pjIpAddress_ = config.getPjIpAddress();

    // 获取调试选项设置
    print_received_comm_ = config.getDebugOption("PRINT_RECEIVED_COMM");              // 是否打印接收到的通信数据
    log_received_comm_ = config.getDebugOption("LOG_RECEIVED_COMM");                  // 是否记录接收到的通信数据
    print_sent_comm_ = config.getDebugOption("PRINT_SENT_COMM");                      // 是否打印发送的通信数据
    log_sent_comm_ = config.getDebugOption("LOG_SENT_COMM");                          // 是否记录发送的通信数据
    log_visible_camera_data_ = config.getDebugOption("LOG_VISIBLE_CAMERA_DATA");      // 是否记录可见光相机数据
    log_visible_camera_status_ = config.getDebugOption("LOG_VISIBLE_CAMERA_STATUS");  // 是否记录可见光相机状态
    log_infrared_camera_data_ = config.getDebugOption("LOG_VISIBLE_CAMERA_DATA");     // 是否记录红外相机数据
    log_infrared_camera_status_ = config.getDebugOption("LOG_VISIBLE_CAMERA_STATUS"); // 是否记录红外相机状态
}

// 初始化网络连接和 RTSP 服务
void NetworkManager::initialize()
{
    // 从配置中获取 RTSP 启用状态
    rtsp_enabled_ = config_.getRTSPEnabled();

    if (rtsp_enabled_)
    {
        // 如果启用了 RTSP，则初始化 RTSP 服务，使用 H264 编码
        theRTSP()->Init(PT_H264);
        Logger::info("RTSP initialized");
    }
    else
    {
        Logger::info("RTSP is disabled");
    }

    // 初始化发送套接字
    sockfd_send = socket(AF_INET, SOCK_DGRAM, 0); // 创建 UDP 套接字
    if (sockfd_send < 0)
    {
        // 套接字创建失败，记录错误
        std::cerr << "Failed to create send socket" << std::endl;
        Logger::error("Failed to create send socket: " + std::string(strerror(errno)));
        return;
    }

    // 获取本地 IP 和 UDP 发送端口
    std::string localIP = config_.getLocalIP();
    udpSendPort_ = config_.getUDPSendPort();

    Logger::info("Local IP: " + localIP_ + ", UDP Send Port: " + std::to_string(udpSendPort_));
}

// 启动接收控制线程
void NetworkManager::startReceivingControl(std::function<void(const uint8_t *, size_t)> processCallback)
{
    Utils::bind_to_cpus({0}); // 将线程绑定到 CPU 0
    Logger::info("Thread-receiveControl bound to CPU 0");
    processCallback_ = std::move(processCallback);                                 // 保存回调函数用于处理接收到的数据
    std::thread receiveControlThread(&NetworkManager::receiveControlFromPJ, this); // 创建接收线程
    receiveControlThread.detach();                                                 // 分离线程，使其在后台独立运行
}

// 启动发送状态线程
void NetworkManager::startSendingStatus()
{
    Utils::bind_to_cpus({0}); // 将线程绑定到 CPU 0
    Logger::info("Thread-sendStatus bound to CPU 0");
    std::thread sendStatusThread(&NetworkManager::sendStatusToPJ, this); // 创建发送状态线程
    sendStatusThread.detach();                                           // 分离线程，使其在后台独立运行
}

// 启动处理可见光序列化数据线程
void NetworkManager::startProcessingVisibleSerial()
{
    Utils::bind_to_cpus({0}); // 将线程绑定到 CPU 0
    Logger::info("Thread-processVisibleSerial bound to CPU 0");
    std::thread processVisibleSerialThread(&NetworkManager::processVisibleSerialThread, this); // 创建处理可见光串行数据线程
    processVisibleSerialThread.detach();                                                       // 分离线程，使其在后台独立运行
}

// 启动处理红外序列化数据线程
void NetworkManager::startProcessingInfraredSerial()
{
    Utils::bind_to_cpus({0}); // 将线程绑定到 CPU 0
    Logger::info("Thread-processInfraredSerial bound to CPU 0");
    std::thread processInfraredSerialThread(&NetworkManager::processInfraredSerialThread, this); // 创建处理红外串行数据线程
    processInfraredSerialThread.detach();                                                        // 分离线程，使其在后台独立运行
}

// 启动准备状态帧线程
void NetworkManager::startPrepareStatusFrameThread()
{
    Utils::bind_to_cpus({0}); // 将线程绑定到 CPU 0
    Logger::info("Thread-startPrepareStatusFrameThread bound to CPU 0");
    std::thread prepareStatusFrameThread(&NetworkManager::prepareStatusFrameThread, this); // 创建准备状态帧线程
    prepareStatusFrameThread.detach();                                                     // 分离线程，使其在后台独立运行
}

// 停止所有线程
void NetworkManager::stop()
{
    shouldStop_ = true; // 设置停止标志，各线程会检查此标志并退出
}

// 从 PJ 接收控制帧的线程函数
void NetworkManager::receiveControlFromPJ()
{
    std::cout << "Starting receiveControlFromPJ function" << std::endl;
    Logger::info("Starting receiveControlFromPJ function");

    // 设置本地地址结构
    struct sockaddr_in laddr;
    laddr.sin_family = AF_INET;                       // 使用 IPv4
    laddr.sin_port = htons(config_.getReceivePort()); // 设置接收端口
    laddr.sin_addr.s_addr = INADDR_ANY;               // 接收任意 IP 地址发来的数据

    // 创建 UDP 接收套接字
    int sockfd_recv = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd_recv < 0)
    {
        // 套接字创建失败，记录错误
        std::cerr << "Failed to create socket" << std::endl;
        Logger::error("Failed to create socket" + std::string(strerror(errno)));
        return;
    }

    std::cout << "Socket created successfully" << std::endl;
    Logger::info("Socket created successfully");

    // 绑定套接字到本地地址
    if (bind(sockfd_recv, (struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        // 绑定失败，记录错误
        std::cerr << "Failed to bind socket" << std::endl;
        Logger::error("Failed to bind socket" + std::string(strerror(errno)));
        perror("bind");
        close(sockfd_recv);
        return;
    }

    std::cout << "Socket bound successfully to port " << std::to_string(config_.getReceivePort()) << std::endl;
    Logger::info("Socket bound successfully to port " + std::to_string(config_.getReceivePort()));

    // 循环接收数据，直到 shouldStop_ 为 true
    while (!shouldStop_)
    {
        // 准备接收缓冲区
        uint8_t control_frame[36];
        memset(control_frame, 0, sizeof(control_frame));
        struct sockaddr_in raddr; // 用于保存发送方地址
        socklen_t raddr_len = sizeof(raddr);

        // 接收数据
        int len = recvfrom(sockfd_recv, control_frame, sizeof(control_frame), 0,
                           (struct sockaddr *)&raddr, &raddr_len);

        if (len > 0)
        {
            // 收到数据，转换为十六进制字符串用于记录
            std::string hexData = Utils::bytesToHexString(control_frame, len);

            // 根据调试选项决定是否打印接收到的数据
            if (print_received_comm_)
            {
                std::cout << "Received control frame: " << hexData << std::endl;
            }

            // 根据调试选项决定是否记录接收到的数据
            if (log_received_comm_)
            {
                Logger::info("Received control frame: " + hexData);
            }

            // 如果设置了回调函数，调用它处理接收到的数据
            if (processCallback_)
            {
                processCallback_(control_frame, len);
            }
        }
        else if (len < 0)
        {
            // 接收数据错误
            std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
            Logger::error("Error receiving data: " + std::string(strerror(errno)));
        }
    }

    // 线程退出时关闭套接字
    std::cout << "Closing socket and exiting receiveControlFromPJ" << std::endl;
    Logger::info("Closing socket and exiting receiveControlFromPJ");
    close(sockfd_recv);
}

// 向 PJ 发送状态帧的线程函数
// 向 PJ 发送状态帧的线程函数
void NetworkManager::sendStatusToPJ()
{
    // 设置远程地址结构
    struct sockaddr_in raddr;
    raddr.sin_family = AF_INET;                                          // 使用 IPv4
    raddr.sin_port = htons(config_.getSendPort());                       // 设置发送端口
    raddr.sin_addr.s_addr = inet_addr(config_.getPjIpAddress().c_str()); // 设置目标 IP 地址

    // ============ 线程启动时间戳 ============
    auto thread_start = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(thread_start.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(thread_start);
    std::stringstream start_time_str;
    start_time_str << std::put_time(std::localtime(&timer), "%Y-%m-%d %H:%M:%S")
                   << '.' << std::setfill('0') << std::setw(3) << ms.count();

    std::cout << "[" << start_time_str.str() << "] Starting sendStatusToPJ function" << std::endl;
    Logger::info("Starting sendStatusToPJ function at " + start_time_str.str());

    // 记录目标 IP 和端口
    std::string destIp = config_.getPjIpAddress();
    int destPort = config_.getSendPort();
    std::cout << "[" << start_time_str.str() << "] Sending to IP: " << destIp 
              << ", Port: " << destPort << std::endl;
    Logger::info("Sending to IP: " + destIp + ", Port: " + std::to_string(destPort));

    // 获取调试模式设置
    bool debug_mode = config_.getDebugOption("status_frame_debug");
    std::map<int, uint8_t> debug_modifications;

    if (debug_mode)
    {
        // 如果启用了调试模式，获取调试修改设置
        debug_modifications = config_.getStatusFrameDebugModifications();
        Logger::info("Debug mode enabled for status frame modifications");
    }

    // ============ 用于统计发送频率 ============
    auto last_stats_time = std::chrono::steady_clock::now();
    int sent_count = 0;
    int error_count = 0;

    // 循环发送状态帧，直到 shouldStop_ 为 true
    while (!shouldStop_)
    {
        // ============ 发送开始时间戳 ============
        auto send_start = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::system_clock::now();
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        timer = std::chrono::system_clock::to_time_t(now);
        std::stringstream time_str;
        time_str << std::put_time(std::localtime(&timer), "%Y-%m-%d %H:%M:%S")
                 << '.' << std::setfill('0') << std::setw(3) << ms.count();

        // 准备状态帧缓冲区
        uint8_t status_frame[138];
        memset(status_frame, 0, sizeof(status_frame));

        {
            // 使用互斥锁保护对状态帧的访问
            std::lock_guard<std::mutex> lock(status_frame_mutex_);
            memcpy(status_frame, g_status_frame, sizeof(status_frame));
        }

        if (debug_mode)
        {
            // 如果启用了调试模式，应用调试修改
            for (const auto &mod : debug_modifications)
            {
                int byte = mod.first;
                uint8_t value = mod.second;
                if (byte >= 0 && byte < 138)
                {
                    status_frame[byte] = value;
                    std::bitset<8> bits(value);
                    Logger::debug("Modified status_frame[" + std::to_string(byte) + "] = " + bits.to_string());
                }
            }
        }

        // 计算校验和
        status_frame[137] = Utils::calculateChecksum(status_frame, 137);

        // 发送状态帧
        int sent_bytes = sendto(sockfd_send, status_frame, sizeof(status_frame), 0,
                                (struct sockaddr *)&raddr, sizeof(raddr));

        // ============ 计算发送耗时 ============
        auto send_end = std::chrono::high_resolution_clock::now();
        auto send_duration = std::chrono::duration_cast<std::chrono::microseconds>(send_end - send_start);

        if (sent_bytes > 0)
        {
            sent_count++;

            // 发送成功，格式化发送的数据用于记录
            std::stringstream indexedHexData;

            for (size_t i = 0; i < sizeof(status_frame); ++i)
            {
                if (i > 0)
                    indexedHexData << " ";
                indexedHexData << "[" << std::setw(3) << std::setfill('0') << std::dec << (i % 1000) << "]"
                               << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(status_frame[i]);
            }

            std::string formattedHexData = indexedHexData.str();

            // 根据调试选项决定是否打印发送的数据
            if (print_sent_comm_)
            {
                std::cout << "[" << time_str.str() << "] Sent status frame (" 
                          << sent_bytes << " bytes, " 
                          << send_duration.count() << "μs): " 
                          << formattedHexData << std::endl;
            }
            // else
            // {
            //     // 即使不打印详细数据，也打印简要信息
            //     std::cout << "[" << time_str.str() << "] Status frame sent: " 
            //               << sent_bytes << " bytes | Duration: " 
            //               << send_duration.count() << "μs" << std::endl;
            // }

            // 根据调试选项决定是否记录发送的数据
            if (log_sent_comm_)
            {
                Logger::info("[" + time_str.str() + "] Sent status frame (" + 
                           std::to_string(sent_bytes) + " bytes, " + 
                           std::to_string(send_duration.count()) + "μs): " + 
                           formattedHexData);
            }
        }
        else
        {
            error_count++;

            // 发送数据错误（带时间戳）
            std::cerr << "[" << time_str.str() << "] Error sending data: " 
                      << strerror(errno) << std::endl;
            Logger::error("[" + time_str.str() + "] Error sending data: " + 
                         std::string(strerror(errno)));
        }

        // ============ 每5秒统计一次发送频率 ============
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_stats_time);
        
        if (elapsed.count() >= 5)
        {
            double send_rate = sent_count / static_cast<double>(elapsed.count());
            double error_rate = error_count / static_cast<double>(elapsed.count());
            
            auto stats_now = std::chrono::system_clock::now();
            ms = std::chrono::duration_cast<std::chrono::milliseconds>(stats_now.time_since_epoch()) % 1000;
            timer = std::chrono::system_clock::to_time_t(stats_now);
            std::stringstream stats_time;
            stats_time << std::put_time(std::localtime(&timer), "%Y-%m-%d %H:%M:%S")
                      << '.' << std::setfill('0') << std::setw(3) << ms.count();
            
            std::cout << "[" << stats_time.str() << "] [Send Stats] "
                      << "Rate: " << std::fixed << std::setprecision(2) << send_rate << " frames/s | "
                      << "Sent: " << sent_count << " | "
                      << "Errors: " << error_count << " | "
                      << "Success: " << std::setprecision(1) 
                      << (sent_count * 100.0 / (sent_count + error_count)) << "%" 
                      << std::endl;
            
            Logger::info("[Send Stats] Rate: " + std::to_string(send_rate) + 
                        " frames/s | Sent: " + std::to_string(sent_count) + 
                        " | Errors: " + std::to_string(error_count));
            
            // 重置计数器
            sent_count = 0;
            error_count = 0;
            last_stats_time = current_time;
        }

        // 控制发送频率，每 40 毫秒发送一次状态命令
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    // ============ 线程退出时间戳 ============
    auto thread_end = std::chrono::system_clock::now();
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(thread_end.time_since_epoch()) % 1000;
    timer = std::chrono::system_clock::to_time_t(thread_end);
    std::stringstream end_time_str;
    end_time_str << std::put_time(std::localtime(&timer), "%Y-%m-%d %H:%M:%S")
                 << '.' << std::setfill('0') << std::setw(3) << ms.count();

    std::cout << "[" << end_time_str.str() << "] Closing socket and exiting sendStatusToPJ" << std::endl;
    Logger::info("Closing socket and exiting sendStatusToPJ at " + end_time_str.str());
}

// 更新工作状态
void NetworkManager::updateWorkStatus()
{
    uint8_t work_status = 0;

    // 检查可见光错误状态，如果有错误设置工作状态位
    if (vic_error_status_ & 0x07)
    {
        work_status |= (1 << 1);
    }

    // 检查红外错误状态，如果有错误设置工作状态位
    if (ir_error_status_ & 0x07)
    {
        work_status |= (1 << 0);
    }

    // 检查激光雷达状态，如果有错误设置工作状态位
    if (lidar_status_ & 0x01)
    {
        work_status |= (1 << 2);
    }

    // 更新工作状态
    work_status_ = work_status;
}

// 处理可见光串行数据的线程函数
void NetworkManager::processVisibleSerialThread()
{
    Logger::info("Starting processVisibleSerialThread function");

    // 获取可见光相机串口设备路径
    std::string serialPort = config_.getVisibleCameraSerialPort();

    // 打开串口设备
    int fd = open(serialPort.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
    {
        // 打开失败，记录错误
        Logger::error("Failed to open visible camera serial port: " + std::string(strerror(errno)));
        return;
    }

    Logger::info("Serial port opened successfully");

    // 配置串口参数
    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD; // 115200 波特率，8 位数据位，本地连接，启用接收
    options.c_iflag = IGNPAR;                         // 忽略奇偶校验错误
    options.c_oflag = 0;                              // 禁用输出处理
    options.c_lflag = 0;                              // 禁用规范模式处理等
    tcflush(fd, TCIFLUSH);                            // 清空输入缓冲区
    tcsetattr(fd, TCSANOW, &options);                 // 应用新的串口设置

    Logger::info("Serial port parameters set");

    // 循环读取串口数据，直到 shouldStop_ 为 true
    while (!shouldStop_)
    {
        char buffer[1024];
        int n = read(fd, buffer, sizeof(buffer)); // 读取串口数据

        if (n > 0)
        {
            // 检查帧头，确认是可见光相机数据
            if (buffer[0] == 0x86 && buffer[1] == 0x55)
            {
                // 根据调试选项决定是否记录接收到的数据
                if (log_visible_camera_data_)
                {
                    std::string hexData = Utils::bytesToHexString(reinterpret_cast<uint8_t *>(buffer), n);
                    Logger::info("Received visible camera data: " + hexData);
                }

                // 更新可见光相机状态
                vic_error_status_ = buffer[2];
                vic_device_status_ = buffer[3];

                // 根据调试选项决定是否记录状态更新
                if (log_visible_camera_status_)
                {
                    Logger::info("Updated status - Error: " + std::to_string(vic_error_status_) +
                                 ", Device: " + std::to_string(vic_device_status_));
                }
            }
        }
        else if (n < 0)
        {
            // 读取错误，但不记录，防止日志过多
            // Logger::error("Error reading from visible serial port: " + std::string(strerror(errno)));
        }

        // 控制读取频率，每 40 毫秒读取一次
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    // 线程退出时关闭串口
    Logger::info("Closing serial port and exiting processVisibleSerialThread");
    close(fd);
}

// 处理红外串行数据的线程函数
void NetworkManager::processInfraredSerialThread()
{
    Logger::info("Starting processInfraredSerialThread function");

    // 获取红外相机串口设备路径
    std::string serialPort = config_.getInfraredCameraSerialPort();

    // 打开串口设备
    int fd = open(serialPort.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
    {
        // 打开失败，记录错误
        Logger::error("Failed to open infrared camera serial port: " + std::string(strerror(errno)));
        return;
    }

    Logger::info("Infrared serial port opened successfully");

    // 配置串口参数
    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD; // 115200 波特率，8 位数据位，本地连接，启用接收
    options.c_iflag = IGNPAR;                         // 忽略奇偶校验错误
    options.c_oflag = 0;                              // 禁用输出处理
    options.c_lflag = 0;                              // 禁用规范模式处理等
    tcflush(fd, TCIFLUSH);                            // 清空输入缓冲区
    tcsetattr(fd, TCSANOW, &options);                 // 应用新的串口设置

    Logger::info("Infrared serial port parameters set");

    // 循环读取串口数据，直到 shouldStop_ 为 true
    while (!shouldStop_)
    {
        char buffer[1024];
        int n = read(fd, buffer, sizeof(buffer)); // 读取串口数据

        if (n > 0)
        {
            // 检查帧头，确认是红外相机数据
            if (buffer[0] == 0xca && buffer[1] == 0x13)
            {
                // 根据调试选项决定是否记录接收到的数据
                if (log_infrared_camera_data_)
                {
                    std::string hexData = Utils::bytesToHexString(reinterpret_cast<uint8_t *>(buffer), n);
                    Logger::info("Received infrared camera data: " + hexData);
                }

                // 更新红外相机状态
                ir_error_status_ = buffer[2];
                ir_device_status_ = buffer[3];

                // 根据调试选项决定是否记录状态更新
                if (log_infrared_camera_status_)
                {
                    Logger::info("Updated infrared status - Error: " + std::to_string(ir_error_status_) +
                                 ", Device: " + std::to_string(ir_device_status_));
                }
            }
        }
        else if (n < 0)
        {
            // 读取错误，但不记录，防止日志过多
            // Logger::error("Error reading from infrared serial port: " + std::string(strerror(errno)));
        }

        // 控制读取频率，每 40 毫秒读取一次
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    // 线程退出时关闭串口
    Logger::info("Closing infrared serial port and exiting processInfraredSerialThread");
    close(fd);
}

// 发送可见光控制信号
void NetworkManager::sendVisibleCtrlSignal(uint8_t command, uint16_t crosshairX, uint16_t crosshairY)
{
    // 获取可见光相机串口设备路径
    std::string serialPort = config_.getVisibleCameraSerialPort();

    // 打开串口设备
    int fd = open(serialPort.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
    {
        // 打开失败，记录错误
        std::cerr << "Failed to open visible camera serial port: " << strerror(errno) << std::endl;
        Logger::error("Failed to open visible camera serial port: " + std::string(strerror(errno)));
        return;
    }

    // 配置串口参数
    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD; // 115200 波特率，8 位数据位，本地连接，启用接收
    options.c_iflag = IGNPAR;                         // 忽略奇偶校验错误
    options.c_oflag = 0;                              // 禁用输出处理
    options.c_lflag = 0;                              // 禁用规范模式处理等
    tcflush(fd, TCIFLUSH);                            // 清空输入缓冲区
    tcsetattr(fd, TCSANOW, &options);                 // 应用新的串口设置

    // 准备命令缓冲区
    uint8_t buffer[10] = {
        0x85, 0xaa, command,                            // 帧头和命令
        static_cast<uint8_t>(crosshairX & 0xFF),        // 准星 X 坐标低字节
        static_cast<uint8_t>((crosshairX >> 8) & 0xFF), // 准星 X 坐标高字节
        static_cast<uint8_t>(crosshairY & 0xFF),        // 准星 Y 坐标低字节
        static_cast<uint8_t>((crosshairY >> 8) & 0xFF), // 准星 Y 坐标高字节
        0, 0};
    buffer[9] = Utils::calculateChecksum(buffer, 9); // 计算校验和

    // 发送命令
    if (write(fd, buffer, sizeof(buffer)) != sizeof(buffer))
    {
        // 发送失败，记录错误
        std::cerr << "Failed to write to visible camera serial port: " << strerror(errno) << std::endl;
        Logger::error("Failed to write to visible camera serial port: " + std::string(strerror(errno)));
    }
    else
    {
        // 发送成功，根据调试选项决定是否记录
        if (log_visible_camera_data_)
        {
            std::string hexData = Utils::bytesToHexString(buffer, sizeof(buffer));
            Logger::info("Sent visible camera control signal: " + hexData);
        }
        if (log_visible_camera_status_)
        {
            Logger::info("Sent visible camera control - Command:" +
                         Utils::bytesToHexString(&command, 1) +
                         ", CrosshairX: " + std::to_string(crosshairX) +
                         ", CrosshairY: " + std::to_string(crosshairY));
        }
    }

    // 关闭串口
    close(fd);
}

// 发送红外控制信号的函数
void NetworkManager::sendInfraredCtrlSignal(uint8_t command, uint16_t crosshairX, uint16_t crosshairY)
{
    // 获取红外相机串口设备路径
    std::string serialPort = config_.getInfraredCameraSerialPort();
    // 打开串口设备
    int fd = open(serialPort.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
    {
        // 打开失败，记录错误
        std::cerr << "Failed to open infrared camera serial port: " << strerror(errno) << std::endl;
        Logger::error("Failed to open infrared camera serial port: " + std::string(strerror(errno)));
        return;
    }

    // 配置串口参数
    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD; // 115200波特率，8数据位，本地模式，使能接收
    options.c_iflag = IGNPAR;                         // 忽略帧错误和奇偶校验错误
    options.c_oflag = 0;                              // 不做输出处理
    options.c_lflag = 0;                              // 非规范模式
    tcflush(fd, TCIFLUSH);                            // 清除接收缓存
    tcsetattr(fd, TCSANOW, &options);                 // 立即应用设置

    // 构造控制命令缓冲区
    uint8_t buffer[8] = {
        0xca, 0x03, command,                            // 帧头和命令字
        static_cast<uint8_t>(crosshairX & 0xFF),        // 准星X坐标低字节
        static_cast<uint8_t>((crosshairX >> 8) & 0xFF), // 准星X坐标高字节
        static_cast<uint8_t>(crosshairY & 0xFF),        // 准星Y坐标低字节
        static_cast<uint8_t>((crosshairY >> 8) & 0xFF)  // 准星Y坐标高字节
    };
    buffer[7] = Utils::calculateChecksum(buffer, 7); // 计算校验和

    // 写入数据到串口
    if (write(fd, buffer, sizeof(buffer)) != sizeof(buffer))
    {
        // 写入失败，记录错误
        std::cerr << "Failed to write to infrared camera serial port: " << strerror(errno) << std::endl;
        Logger::error("Failed to write to infrared camera serial port: " + std::string(strerror(errno)));
    }
    else
    {
        // 写入成功，根据调试选项决定是否记录日志
        if (log_infrared_camera_data_)
        {
            std::string hexData = Utils::bytesToHexString(buffer, sizeof(buffer));
            Logger::info("Sent infrared camera control signal: " + hexData);
        }
        if (log_infrared_camera_status_)
        {
            Logger::info("Sent infrared camera control - Command: " +
                         Utils::bytesToHexString(&command, 1) +
                         ", CrosshairX: " + std::to_string(crosshairX) +
                         ", CrosshairY: " + std::to_string(crosshairY));
        }
    }

    // 关闭串口
    close(fd);
}

// 状态帧准备线程函数，负责填充状态数据
void NetworkManager::prepareStatusFrameThread()
{
    // 循环运行直到收到停止信号
    while (!shouldStop_)
    {
        // 更新各种状态数据
        boot_time_ = Utils::getSystemUptime();                               // 获取系统运行时间
        updateWorkStatus();                                                  // 更新工作状态
        detect_result_group_t detection_results = processDetectionResults(); // 处理目标检测结果

        // std::mutex status_frame_mutex_; // 创建互斥锁保护状态帧访问

        // 初始化状态帧缓冲区为0
        memset(g_status_frame, 0, sizeof(g_status_frame));

        // 填充状态帧头信息
        g_status_frame[0] = 0x27; // 帧头第一字节
        g_status_frame[1] = 0x02; // 帧头第二字节
        g_status_frame[2] = 0x8a; // 数据长度

        // 填充系统运行时间
        memcpy(g_status_frame + 3, &boot_time_, sizeof(boot_time_));

        // 填充各种状态信息
        g_status_frame[5] = blend_status_;       // 混合状态
        g_status_frame[6] = work_status_;        // 工作状态
        g_status_frame[7] = lidar_status_;       // 激光雷达状态
        g_status_frame[8] = ir_error_status_;    // 红外错误状态
        g_status_frame[9] = ir_device_status_;   // 红外设备状态
        g_status_frame[10] = vic_error_status_;  // 可见光错误状态
        g_status_frame[11] = vic_device_status_; // 可见光设备状态
        // g_status_frame[12] = collision_warning_; // 碰撞警告标志，后面会更新

        // 填充预设角度信息
        memcpy(g_status_frame + 13, &preset_angle_, sizeof(preset_angle_));
        // 填充岸距离信息
        memcpy(g_status_frame + 15, &shore_distance_, sizeof(shore_distance_));

        // 更新目标数量，最多5个目标
        uint8_t target_num = std::min(static_cast<uint8_t>(detection_results.count), static_cast<uint8_t>(5));
        g_status_frame[17] = target_num;

        // 初始化最小非零距离变量用于碰撞警告计算
        float min_non_zero_distance = std::numeric_limits<float>::max();

        // 填充每个目标的详细信息，每个目标占22字节
        for (int i = 0; i < target_num; i++)
        {
            int offset = 18 + i * 22;                          // 计算当前目标在状态帧中的偏移位置
            const auto &result = detection_results.results[i]; // 获取当前目标检测结果

            // 设置目标ID，加上配置的目标偏置
            g_status_frame[offset] = i + config_.getTargetBias();

            // 根据目标名称确定目标类型
            g_status_frame[offset + 1] = getTargetTypeFromName(result.name);

            // 计算目标方位角
            uint16_t target_azimuth = calculateTargetAzimuth(&result.box);
            // 将方位角写入状态帧
            *(uint16_t *)(g_status_frame + offset + 2) = target_azimuth;

            // 计算目标俯仰角
            int16_t target_pitch = calculateTargetPitch(&result.box);
            // 将俯仰角写入状态帧
            *(int16_t *)(g_status_frame + offset + 4) = target_pitch;

            // 计算目标平均距离并应用偏置
            float avg_distance = calculateAverageDistance(aligned_lidar_for_fusion_, result.box) - config_.getDistanceBias();

            
            // 确保距离不小于0
            avg_distance = std::max(avg_distance, 0.0f);

            // 计算目标实际长度和宽度
            uint16_t target_length = calculateActualSize(result.box.right - result.box.left, avg_distance, VIS_FOCAL_LENGTH, VIS_PIXEL_SIZE);
            uint16_t target_width = calculateActualSize(result.box.bottom - result.box.top, avg_distance, VIS_FOCAL_LENGTH, VIS_PIXEL_SIZE);
            // 将长度和宽度写入状态帧
            *(uint16_t *)(g_status_frame + offset + 6) = target_length;
            *(uint16_t *)(g_status_frame + offset + 8) = target_width;

            // 设置目标在图像中的位置和大小
            *(uint16_t *)(g_status_frame + offset + 10) = result.box.left;                    // 左边界
            *(uint16_t *)(g_status_frame + offset + 12) = result.box.top;                     // 上边界
            *(uint16_t *)(g_status_frame + offset + 14) = result.box.right - result.box.left; // 宽度
            *(uint16_t *)(g_status_frame + offset + 16) = result.box.bottom - result.box.top; // 高度

            // 设置实测目标距离，转换为0.1m精度
            uint16_t target_distance = avg_distance * 10;
            *(uint16_t *)(g_status_frame + offset + 18) = target_distance;

            // 设置识别置信度，转换为0.01精度
            uint16_t target_confidence = result.prop * 100;
            *(uint16_t *)(g_status_frame + offset + 20) = target_confidence;

            // 更新最小非零距离，用于后续的碰撞警告计算
            if (avg_distance > 0 && avg_distance < min_non_zero_distance)
            {
                min_non_zero_distance = avg_distance;
            }

            // 根据最小非零距离更新碰撞警告状态
            if (min_non_zero_distance <= 5.0)
            {
                collision_warning_ = 0x01; // 01: 特别接近5m
            }
            else if (min_non_zero_distance <= 30.0)
            {
                collision_warning_ = 0x00; // 00: 接近 小于等于30m
            }
            else
            {
                collision_warning_ = 0x02; // 10: 正常 大于等于30m
            }

            // 如果没有检测到有效距离，设置为正常状态
            if (min_non_zero_distance == std::numeric_limits<float>::max())
            {
                collision_warning_ = 0x02; // 10: 正常 大于等于30m
            }
        }

        // 更新碰撞警告字段
        g_status_frame[12] = collision_warning_;

        // 计算并填充校验和
        g_status_frame[137] = Utils::calculateChecksum(g_status_frame, 137);

        // 控制循环速率，每40毫秒更新一次状态帧
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
}

// // 分包发送数据的底层函数
// void NetworkManager::sendpkg(const char *data, int size)
// {
//     // 设置目标地址结构
//     struct sockaddr_in raddr;
//     raddr.sin_family = AF_INET;                              // IPv4地址族
//     raddr.sin_port = htons(udpSendPort_);                    // 设置目标端口
//     raddr.sin_addr.s_addr = inet_addr(pjIpAddress_.c_str()); // 设置目标IP地址

//     // 定义包格式常量
//     const int HEADSIZE = 2;                                 // 头部大小
//     const int TAILSIZE = 2;                                 // 尾部大小
//     const int IDXTOTALSIZE = 4;                             // 索引和总包数大小
//     const int AUXSIZE = HEADSIZE + TAILSIZE + IDXTOTALSIZE; // 辅助信息总大小
//     const int BLKSIZE = 1400;                               // 每个数据包的最大大小
//     const int maxblksize = BLKSIZE - AUXSIZE;               // 每个包中实际数据的最大大小

//     // 创建发送缓冲区
//     std::vector<char> buffer(BLKSIZE);

//     int remain = size;                                // 剩余未发送数据大小
//     int pkgidx = 0;                                   // 当前包索引
//     int total = (size + maxblksize - 1) / maxblksize; // 计算总包数

//     // 循环发送所有数据包
//     while (remain > 0)
//     {
//         int bsz = std::min(remain, maxblksize); // 当前包实际数据大小
//         char *pbuf = &buffer[0];                // 缓冲区指针

//         // 填充包头
//         *pbuf++ = 0xAB; // 包头第一字节
//         *pbuf++ = 0xEC; // 包头第二字节

//         // 填充包索引
//         *pbuf++ = pkgidx & 0xFF;
//         *pbuf++ = (pkgidx >> 8) & 0xFF;

//         // 填充总包数
//         *pbuf++ = total & 0xFF;
//         *pbuf++ = (total >> 8) & 0xFF;

//         // 复制实际数据
//         memcpy(pbuf, data + pkgidx * maxblksize, bsz);
//         pbuf += bsz;

//         // 填充包尾
//         *pbuf++ = 0xDB; // 包尾第一字节
//         *pbuf++ = 0xFE; // 包尾第二字节

//         // 发送数据包
//         int ret = sendto(sockfd_send, &buffer[0], bsz + AUXSIZE, 0, (struct sockaddr *)&raddr,
//                          sizeof(struct sockaddr_in));
//         if (ret < 0)
//         {
//             // 发送失败，退出循环
//             break;
//         }

//         // 更新剩余数据大小和包索引
//         remain -= bsz;
//         ++pkgidx;

//         // 每发送40个数据包，休眠1毫秒，防止发送过快造成网络拥塞
//         if (pkgidx % 40 == 0)
//         {
//             std::this_thread::sleep_for(std::chrono::milliseconds(1));
//         }
//     }
// }



/**
 * 分包发送数据的底层函数
 * @param data 要发送的数据指针
 * @param size 数据大小
 * @param packet_interval 每发送多少个数据包后休眠一次（默认40）
 * @param sleep_duration_ms 休眠时间，单位毫秒（默认1毫秒）
 */
void NetworkManager::sendpkg(const char *data, int size, int packet_interval, int sleep_duration_ms)
{
    // 设置目标地址结构
    struct sockaddr_in raddr;
    raddr.sin_family = AF_INET;                              // IPv4地址族
    raddr.sin_port = htons(udpSendPort_);                    // 设置目标端口
    raddr.sin_addr.s_addr = inet_addr(pjIpAddress_.c_str()); // 设置目标IP地址

    // 定义包格式常量
    const int HEADSIZE = 2;                                 // 头部大小
    const int TAILSIZE = 2;                                 // 尾部大小
    const int IDXTOTALSIZE = 4;                             // 索引和总包数大小
    const int AUXSIZE = HEADSIZE + TAILSIZE + IDXTOTALSIZE; // 辅助信息总大小
    const int BLKSIZE = 1400;                               // 每个数据包的最大大小
    const int maxblksize = BLKSIZE - AUXSIZE;               // 每个包中实际数据的最大大小

    // 创建发送缓冲区
    std::vector<char> buffer(BLKSIZE);

    int remain = size;                                // 剩余未发送数据大小
    int pkgidx = 0;                                   // 当前包索引
    int total = (size + maxblksize - 1) / maxblksize; // 计算总包数

    // 参数有效性检查
    if (packet_interval <= 0) {
        Logger::warning("Invalid packet_interval, using default value 40");
        packet_interval = 40;
    }
    if (sleep_duration_ms < 0) {
        Logger::warning("Invalid sleep_duration_ms, using default value 1");
        sleep_duration_ms = 1;
    }

    Logger::debug("sendpkg parameters: packet_interval=" + std::to_string(packet_interval) + 
                  ", sleep_duration_ms=" + std::to_string(sleep_duration_ms) + 
                  ", total_packets=" + std::to_string(total));

    // 循环发送所有数据包
    while (remain > 0)
    {
        int bsz = std::min(remain, maxblksize); // 当前包实际数据大小
        char *pbuf = &buffer[0];                // 缓冲区指针

        // 填充包头
        *pbuf++ = 0xAB; // 包头第一字节
        *pbuf++ = 0xEC; // 包头第二字节

        // 填充包索引
        *pbuf++ = pkgidx & 0xFF;
        *pbuf++ = (pkgidx >> 8) & 0xFF;

        // 填充总包数
        *pbuf++ = total & 0xFF;
        *pbuf++ = (total >> 8) & 0xFF;

        // 复制实际数据
        memcpy(pbuf, data + pkgidx * maxblksize, bsz);
        pbuf += bsz;

        // 填充包尾
        *pbuf++ = 0xDB; // 包尾第一字节
        *pbuf++ = 0xFE; // 包尾第二字节

        // 发送数据包
        int ret = sendto(sockfd_send, &buffer[0], bsz + AUXSIZE, 0, (struct sockaddr *)&raddr,
                         sizeof(struct sockaddr_in));
        if (ret < 0)
        {
            // 发送失败，记录错误并退出循环
            Logger::error("sendto failed at packet " + std::to_string(pkgidx) + ": " + std::string(strerror(errno)));
            break;
        }

        // 更新剩余数据大小和包索引
        remain -= bsz;
        ++pkgidx;

        // 根据配置的间隔进行休眠，防止发送过快造成网络拥塞
        if (packet_interval > 0 && pkgidx % packet_interval == 0)
        {
            if (sleep_duration_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_duration_ms));
            }
        }
    }

    Logger::debug("sendpkg completed: sent " + std::to_string(pkgidx) + " packets");
}





/**
 * 高级数据包发送函数，同时支持UDP和RTSP发送
 * @param data 要发送的数据指针
 * @param size 数据大小
 * @param packet_interval 每发送多少个数据包后休眠一次（默认40）
 * @param sleep_duration_ms 休眠时间，单位毫秒（默认1毫秒）
 */
void NetworkManager::sendPackage(const char *data, int size, int packet_interval, int sleep_duration_ms)
{
    // 参数有效性检查
    if (!data || size <= 0) {
        Logger::warning("Invalid data or size for sendPackage");
        return;
    }
    
    bool data_sent = false;  // 标记是否有数据被发送
    
    // 发送UDP数据包，使用可配置的发送参数
    try {
        sendpkg(data, size, packet_interval, sleep_duration_ms);
        data_sent = true;
        Logger::debug("UDP package sent successfully, size: " + std::to_string(size) + 
                      ", packet_interval: " + std::to_string(packet_interval) + 
                      ", sleep_duration: " + std::to_string(sleep_duration_ms) + "ms");
    } catch (const std::exception& e) {
        Logger::error("UDP send failed: " + std::string(e.what()));
    }
    
    // 条件发送RTSP流数据
    if (rtsp_enabled_) {
        try {
            theRTSP()->PushStream(0, reinterpret_cast<uint8_t *>(const_cast<char *>(data)), size);
            data_sent = true;
            Logger::debug("RTSP stream pushed successfully, size: " + std::to_string(size));
        } catch (const std::exception& e) {
            Logger::error("RTSP push failed: " + std::string(e.what()));
        }
    } else {
        Logger::debug("RTSP streaming disabled, skipping RTSP transmission");
    }
    
    // 警告：如果两个传输方式都被禁用
    if (!data_sent) {
        Logger::warning("No transmission method enabled - data package discarded");
    }
}




// // 高级数据包发送函数，同时支持UDP和RTSP发送
// void NetworkManager::sendPackage(const char *data, int size)
// {
//     // 参数有效性检查
//     if (!data || size <= 0) {
//         Logger::warning("Invalid data or size for sendPackage");
//         return;
//     }
    
//     bool data_sent = false;  // 标记是否有数据被发送
    
//     sendpkg(data, size);

//     // // 条件发送UDP数据
//     // if (udp_enabled_) {
//     //     try {
//     //         sendpkg(data, size);
//     //         data_sent = true;
//     //         Logger::debug("UDP package sent successfully, size: " + std::to_string(size));
//     //     } catch (const std::exception& e) {
//     //         Logger::error("UDP send failed: " + std::string(e.what()));
//     //     }
//     // } else {
//     //     Logger::debug("UDP sending disabled, skipping UDP transmission");
//     // }
    
//     // 条件发送RTSP流数据
//     if (rtsp_enabled_) {
//         try {
//             theRTSP()->PushStream(0, reinterpret_cast<uint8_t *>(const_cast<char *>(data)), size);
//             data_sent = true;
//             Logger::debug("RTSP stream pushed successfully, size: " + std::to_string(size));
//         } catch (const std::exception& e) {
//             Logger::error("RTSP push failed: " + std::string(e.what()));
//         }
//     } else {
//         Logger::debug("RTSP streaming disabled, skipping RTSP transmission");
//     }
    
//     // 警告：如果两个传输方式都被禁用
//     if (!data_sent) {
//         Logger::warning("No transmission method enabled - data package discarded");
//     }
// }
// 设置混合状态
void NetworkManager::setBlendStatusState(uint8_t fusion_format)
{
    blend_status_ = fusion_format;
}

// 设置激光雷达状态
void NetworkManager::setLidarStatusState(uint8_t lidar_status)
{
    lidar_status_ = lidar_status;
}

// 设置检测结果
void NetworkManager::setDetectionResults(detect_result_group_t detect_results)
{
    detect_results_ = detect_results;
}

// 设置角度码状态
void NetworkManager::setAngleCodeState(uint16_t angle_code)
{
    preset_angle_ = angle_code;
}

// 设置距离码状态
void NetworkManager::setDistanceCodeState(uint16_t distance_code)
{
    shore_distance_ = distance_code;
}

// 处理目标检测结果，过滤不符合条件的目标
detect_result_group_t NetworkManager::processDetectionResults()
{
    // 初始化结果结构
    detect_result_group_t result = {detect_results_.id, 0, {}};
    // 获取严格检测模式设置
    bool strict_mode = config_.getStrictDetectionMode();

    // 遍历所有检测到的目标
    for (int i = 0; i < detect_results_.count; i++)
    {
        const detect_result_t &det_result = detect_results_.results[i];

        // 根据目标名称确定目标类型
        TargetType target_type = determineTargetType(det_result.name);

        // 检查目标是否有效
        bool is_valid = true;

        // 在严格模式下，过滤掉人员目标
        if (strict_mode)
        {
            if (target_type == TargetType::Person)
            {
                is_valid = false;
            }
        }

        // 如果目标类型有效且通过了有效性检查，则添加到结果中
        if (target_type != TargetType::Invalid && is_valid && result.count < OBJ_NUMB_MAX_SIZE)
        {
            result.results[result.count] = det_result;
            result.count++;
        }
    }

    return result;
}

// 根据目标名称确定目标类型
TargetType NetworkManager::determineTargetType(const char *name)
{
    // 在目标类型映射表中查找
    auto it = targetTypeMap.find(name);
    // 如果找到返回对应类型，否则返回无效类型
    return (it != targetTypeMap.end()) ? it->second : TargetType::Invalid;
}

// 检查目标置信度是否有效
bool NetworkManager::isConfidenceValid(TargetType type, float prop)
{
    // 对于军舰或人员目标，置信度需要大于等于0.90才有效
    return !(type == TargetType::Warship || type == TargetType::Person) || prop >= 0.90;
}

// 根据目标名称获取对应的目标类型码
uint8_t NetworkManager::getTargetTypeFromName(const char *name)
{
    // 匹配各种目标类型
    if (strcmp(name, "warship") == 0) // 军舰
        return 0;
    else if (strcmp(name, "cargoship") == 0) // 货船
        return 1;
    else if (strcmp(name, "fishing_boat") == 0) // 渔船
        return 2;
    else if (strcmp(name, "cruise_ship") == 0) // 邮轮
        return 3;
    else if (strcmp(name, "small_boat") == 0) // 小船
        return 4;
    else if (strcmp(name, "marker_float") == 0) // 浮标
        return 5;
    else if (strcmp(name, "quay") == 0) // 码头
        return 6;
    else if (strcmp(name, "person") == 0) // 人员
        return 7;
    // 其他类型可以根据需要取消注释
    // else if (strcmp(name, "not_interested") == 0)  // 不感兴趣
    //     return 8;
    // else if (strcmp(name, "other_ship") == 0)  // 其他船只
    //     return 9;
    else
        return 255; // 无效类型
}

// 计算目标方位角
uint16_t NetworkManager::calculateTargetAzimuth(const BOX_RECT *box)
{
    // 计算目标框中心的X坐标
    float center_x = (box->left + box->right) / 2.0f;
    // 将图像坐标转换为方位角，假设视场角为104.6度
    float azimuth = (center_x / config_.getImageWidth("vic")) * 104.6f - 52.3f;

    // 添加配置的偏移量
    azimuth += config_.getOffset();
    // 确保方位角在0-360度范围内
    azimuth = fmod(azimuth + 360.0f, 360.0f);

    // 转换为0.01度精度的整数值
    return static_cast<int16_t>(std::round(azimuth * 100));
}

// 计算目标俯仰角
int16_t NetworkManager::calculateTargetPitch(const BOX_RECT *box)
{
    // 计算目标框中心的Y坐标
    float center_y = (box->top + box->bottom) / 2.0f;
    // 将图像坐标转换为俯仰角，假设垂直视场角为58.8度
    float pitch = (center_y / config_.getImageHeight("vic")) * 58.8f - 29.4f;

    // 转换为0.01度精度的整数值
    return static_cast<int16_t>(std::round(pitch * 100));
}

// 计算目标实际尺寸
int16_t NetworkManager::calculateActualSize(float pixel_count, float distance, float focal_length, float pixel_size)
{
    // 如果距离无效，返回0
    if (distance <= 0)
    {
        return 0;
    }
    // 使用针孔相机模型计算实际尺寸
    float actual_size = pixel_size * pixel_count * distance / focal_length;
    // 除以2调整尺寸
    actual_size = actual_size / 2;
    // 转换为0.1米精度的整数值
    return static_cast<int16_t>(actual_size * 10);
}

// 计算目标平均距离
float NetworkManager::calculateAverageDistance(const cv::Mat &distance_img, const BOX_RECT &box)
{
    // 开始计时，用于性能分析
    auto start_time = std::chrono::high_resolution_clock::now();

    // 输入参数有效性检查
    if (distance_img.empty() || box.left >= box.right || box.top >= box.bottom)
    {
        return 0.0f; // 无效输入，返回0
    }

    float avg_distance = 0; // 平均距离
    int count = 0;          // 有效点计数

    // 获取目标框的位置和大小
    int x = box.left;
    int y = box.top;
    int rect_width = box.right - box.left;
    int rect_height = box.bottom - box.top;

    // 边界检查
    if (x < 0 || y < 0 || x >= distance_img.cols || y >= distance_img.rows)
    {
        std::cerr << "Box coordinates out of image bounds" << std::endl;
        return 0.0f;
    }

    // 计算内部区域大小，用于提取距离数据
    cv::Size region = cv::Size(rect_width, rect_height);
    // region.width = std::max(2, region.width);   // 确保宽度至少为2
    // region.height = std::max(2, region.height); // 确保高度至少为2
    region.width = std::max(2, region.width / 5);   // 确保宽度至少为2
    region.height = std::max(2, region.height / 5); // 确保高度至少为2

    // 计算内部区域的边界
    int inner_left = std::max(0, x + rect_width / 2 - region.width / 2);
    int inner_top = std::max(0, y + rect_height / 2 - region.height / 2);
    int inner_right = std::min(x + rect_width / 2 + region.width / 2, distance_img.cols);
    int inner_bottom = std::min(y + rect_height / 2 + region.height / 2, distance_img.rows);

    // 内部区域有效性检查
    if (inner_left >= inner_right || inner_top >= inner_bottom)
    {
        std::cerr << "Invalid inner region" << std::endl;
        return 0.0f;
    }

    // 遍历内部区域的每个像素
    for (int i = inner_top; i < inner_bottom; i++)
    {
        // 行索引检查
        if (i < 0 || i >= distance_img.rows)
        {
            std::cerr << "Row index out of bounds: " << i << std::endl;
            continue;
        }
        // 获取当前行的距离数据指针
        const float *distance_row = distance_img.ptr<float>(i);

        // 遍历当前行的每个像素
        for (int j = inner_left; j < inner_right; j++)
        {
            // 列索引检查
            if (j < 0 || j >= distance_img.cols)
            {
                std::cerr << "Column index out of bounds: " << j << std::endl;
                continue;
            }
            // 获取当前像素的距离值
            float distance = distance_row[j];
            // 如果距离值有效（大于0），则累加
            if (distance > 0)
            {
                avg_distance += distance;
                count++;
            }
        }
    }

    // 计算平均距离
    if (count > 0)
    {
        avg_distance /= count;
    }

    // 计算处理耗时
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    return avg_distance;
}

// 设置对齐后的激光雷达数据，用于融合处理
void NetworkManager::setAlignedLidarForFusion(const cv::Mat &aligned_lidar)
{
    // 克隆输入矩阵以确保数据所有权
    aligned_lidar_for_fusion_ = aligned_lidar.clone();
}