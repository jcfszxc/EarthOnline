/**
 * @file NetworkManager.h
 * @brief 网络管理器头文件，负责处理网络通信、目标检测数据传输和设备状态管理
 *
 * 该类提供了网络通信功能，包括：
 * 1. UDP通信用于发送状态和接收控制信号
 * 2. 处理可见光和红外相机的串行数据
 * 3. 目标检测结果的格式化和传输
 * 4. 设备状态监控和报告
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <cstdint>    // 提供固定大小的整数类型
#include <functional> // 用于回调函数
#include <atomic>     // 提供原子操作
#include "Config.h"   // 配置管理
#include "Utils.h"    // 工具函数
#include "Logger.h"   // 日志功能
#include "Detector.h" // 目标检测器

// 网络相关头文件
#include <sys/types.h>  // 基本系统数据类型
#include <sys/socket.h> // 套接字定义
#include <netdb.h>      // 网络数据库操作
#include <ifaddrs.h>    // 网络接口地址
#include <arpa/inet.h>  // IP地址转换函数
#include "RTSPPush.h"   // RTSP推流功能

/**
 * @enum TargetType
 * @brief 使用枚举来表示目标类型
 */
enum class TargetType : uint8_t
{
    Warship = 0,  // 军舰
    Cargoship,    // 货船
    FishingBoat,  // 渔船
    CruiseShip,   // 游轮
    SmallBoat,    // 小船
    MarkerFloat,  // 浮标
    Quay,         // 码头
    Person,       // 人
    Invalid = 255 // 无效目标
};

// 目标类型字符串映射表，静态成员初始化
const std::unordered_map<std::string, TargetType> targetTypeMap = {
    {"warship", TargetType::Warship},          // 军舰
    {"cargoship", TargetType::Cargoship},      // 货船
    {"fishing_boat", TargetType::FishingBoat}, // 渔船
    {"cruise_ship", TargetType::CruiseShip},   // 游轮
    {"small_boat", TargetType::SmallBoat},     // 小船
    {"marker_float", TargetType::MarkerFloat}, // 浮标
    {"quay", TargetType::Quay},                // 码头
    {"person", TargetType::Person}             // 人
};

/**
 * @class NetworkManager
 * @brief 网络管理器类，负责处理设备间的通信和状态管理
 */
class NetworkManager
{
public:
    /**
     * @brief 构造函数
     * @param config 配置对象
     */
    NetworkManager(const Config &config);

    /**
     * @brief 初始化网络管理器
     */
    void initialize();

    /**
     * @brief 开始接收控制信号
     * @param processCallback 处理接收数据的回调函数
     */
    void startReceivingControl(std::function<void(const uint8_t *, size_t)> processCallback);

    /**
     * @brief 开始发送状态信息
     */
    void startSendingStatus();

    /**
     * @brief 开始处理可见光相机的串行数据
     */
    void startProcessingVisibleSerial();

    /**
     * @brief 开始处理红外相机的串行数据
     */
    void startProcessingInfraredSerial();

    /**
     * @brief 开始准备状态帧的线程
     */
    void startPrepareStatusFrameThread();

    /**
     * @brief 发送可见光控制信号
     * @param command 命令
     * @param crosshairX 十字准线X坐标
     * @param crosshairY 十字准线Y坐标
     */
    void sendVisibleCtrlSignal(uint8_t command, uint16_t crosshairX, uint16_t crosshairY);

    /**
     * @brief 发送红外控制信号
     * @param command 命令
     * @param crosshairX 十字准线X坐标
     * @param crosshairY 十字准线Y坐标
     */
    void sendInfraredCtrlSignal(uint8_t command, uint16_t crosshairX, uint16_t crosshairY);

    /**
     * @brief 发送数据包
     * @param data 数据
     * @param size 数据大小
     */
    // void sendPackage(const char *data, int size);
    void sendPackage(const char *data, int size, int packet_interval = 40, int sleep_duration_ms = 1);

    /**
     * @brief 设置融合状态
     * @param fusion_format 融合格式
     */
    void setBlendStatusState(uint8_t fusion_format);

    /**
     * @brief 设置激光雷达状态
     * @param lidar_status 激光雷达状态
     */
    void setLidarStatusState(uint8_t lidar_status);

    /**
     * @brief 设置检测结果
     * @param lidar_status 检测结果组
     */
    void setDetectionResults(detect_result_group_t lidar_status);

    /**
     * @brief 设置用于融合的对齐激光雷达数据
     * @param aligned_lidar 对齐的激光雷达数据
     */
    void setAlignedLidarForFusion(const cv::Mat &aligned_lidar);

    /**
     * @brief 设置角度码状态
     * @param angle_code 角度码
     */
    void setAngleCodeState(uint16_t angle_code);

    /**
     * @brief 设置距离码状态
     * @param distance_code 距离码
     */
    void setDistanceCodeState(uint16_t distance_code);

    /**
     * @brief 停止网络管理器
     */
    void stop();

private:
    /**
     * @brief 从PJ接收控制信号
     */
    void receiveControlFromPJ();

    /**
     * @brief 向PJ发送状态信息
     */
    void sendStatusToPJ();

    /**
     * @brief 更新工作状态
     */
    void updateWorkStatus();

    /**
     * @brief 处理可见光相机串行数据的线程
     */
    void processVisibleSerialThread();

    /**
     * @brief 处理红外相机串行数据的线程
     */
    void processInfraredSerialThread();

    /**
     * @brief 准备状态帧的线程
     */
    void prepareStatusFrameThread();

    /**
     * @brief 发送数据包
     * @param data 数据
     * @param size 数据大小
     */
    // void sendpkg(const char *data, int size);
    
    void sendpkg(const char *data, int size, int packet_interval = 40, int sleep_duration_ms = 1);

    const Config &config_;                                         // 配置对象引用
    std::atomic<bool> shouldStop_;                                 // 停止标志
    std::function<void(const uint8_t *, size_t)> processCallback_; // 处理回调函数
    int sockfd_send;                                               // 发送套接字
    std::string pjIpAddress_;                                      // PJ的IP地址

    // 状态帧成员变量
    uint16_t boot_time_;         // 启动时间
    uint8_t blend_status_;       // 融合状态
    uint8_t work_status_;        // 工作状态
    uint8_t lidar_status_;       // 激光雷达状态
    uint8_t ir_error_status_;    // 红外错误状态
    uint8_t ir_device_status_;   // 红外设备状态
    uint8_t vic_error_status_;   // 可见光错误状态
    uint8_t vic_device_status_;  // 可见光设备状态
    uint8_t collision_warning_;  // 碰撞警告
    uint16_t preset_angle_;      // 预设角度
    uint16_t shore_distance_;    // 岸距
    uint8_t target_num_;         // 目标数量
    uint8_t g_status_frame[138]; // 状态帧缓冲区

    bool print_received_comm_;        // 打印接收的通信
    bool log_received_comm_;          // 记录接收的通信
    bool print_sent_comm_;            // 打印发送的通信
    bool log_sent_comm_;              // 记录发送的通信
    bool log_visible_camera_data_;    // 记录可见光相机数据
    bool log_visible_camera_status_;  // 记录可见光相机状态
    bool log_infrared_camera_data_;   // 记录红外相机数据
    bool log_infrared_camera_status_; // 记录红外相机状态

    std::string localIP_;                  // 本地IP地址
    int udpSendPort_;                      // UDP发送端口
    detect_result_group_t detect_results_; // 检测结果组

    /**
     * @brief 处理检测结果
     * @return 处理后的检测结果组
     */
    detect_result_group_t processDetectionResults();

    /**
     * @brief 确定目标类型
     * @param name 目标名称
     * @return 目标类型
     */
    TargetType determineTargetType(const char *name);

    /**
     * @brief 判断置信度是否有效
     * @param type 目标类型
     * @param prop 置信度
     * @return 是否有效
     */
    bool isConfidenceValid(TargetType type, float prop);

    std::mutex status_frame_mutex_; // 状态帧互斥锁

    /**
     * @brief 从名称获取目标类型
     * @param name 目标名称
     * @return 目标类型的数值表示
     */
    uint8_t getTargetTypeFromName(const char *name);

    /**
     * @brief 计算目标方位角
     * @param box 目标边界框
     * @return 方位角
     */
    uint16_t calculateTargetAzimuth(const BOX_RECT *box);

    /**
     * @brief 计算目标俯仰角
     * @param box 目标边界框
     * @return 俯仰角
     */
    int16_t calculateTargetPitch(const BOX_RECT *box);

    /**
     * @brief 计算实际尺寸
     * @param pixel_count 像素数
     * @param distance 距离
     * @param focal_length 焦距
     * @param pixel_size 像素大小
     * @return 实际尺寸
     */
    int16_t calculateActualSize(float pixel_count, float distance, float focal_length, float pixel_size);

    /**
     * @brief 计算平均距离
     * @param distance_img 距离图像
     * @param box 目标边界框
     * @return 平均距离
     */
    float calculateAverageDistance(const cv::Mat &distance_img, const BOX_RECT &box);

    cv::Mat aligned_lidar_for_fusion_; // 用于融合的对齐激光雷达数据

    const float IR_FOCAL_LENGTH = 5.8;  // 红外焦距，单位：mm
    const float VIS_FOCAL_LENGTH = 3.0; // 可见光焦距，单位：mm
    const float IR_PIXEL_SIZE = 12e-3;  // 红外像元大小，单位：mm
    const float VIS_PIXEL_SIZE = 4e-3;  // 可见光像元大小，单位：mm

    bool rtsp_enabled_; // RTSP是否启用
    bool udp_enabled_;      // 新增的UDP开关
};

#endif // NETWORK_MANAGER_H