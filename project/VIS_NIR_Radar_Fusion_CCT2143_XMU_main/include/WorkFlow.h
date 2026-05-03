#ifndef WORKFLOW_H  // 防止头文件重复包含的宏定义开始
#define WORKFLOW_H

/**
 * WorkFlow 类 - 多传感器融合系统控制流程
 * 
 * 功能概述:
 * 该类实现了一个完整的工作流程，包括可见光相机、红外相机和激光雷达(LiDAR)数据的采集、处理和融合。
 * 系统支持多种显示模式、图像融合格式、视频编码和网络传输功能。
 * 
 * 主要组件:
 * 1. 视频流处理 - 使用GStreamer框架处理可见光和红外视频流
 * 2. 激光雷达数据处理 - 获取深度信息并与视频融合
 * 3. 图像融合 - 多种融合模式的实现(RGA操作)
 * 4. 目标检测 - 基于深度学习的检测功能
 * 5. 数据传输 - 网络传输编码后的视频流
 * 6. 状态控制 - 设备状态和模式控制
 */

#include "Config.h"            // 配置参数管理
#include "Logger.h"            // 日志系统
#include "WorkFlowUtils.h"     // 工作流程辅助工具
#include "LidarManualSelection.h" // 激光雷达手动选择功能
#include "NetworkManager.h"    // 网络管理
#include <cstdint>             // 标准整数类型定义
#include <gst/gst.h>           // GStreamer核心库
#include <gst/app/gstappsink.h> // GStreamer应用接收器
#include "RGAOperations.h"     // RGA操作(可能是硬件加速图像处理)
#include <condition_variable>  // 条件变量，用于线程同步
#include "Detector.h"          // 目标检测器
#include "GetLidarData_CX128S2.h" // 特定型号激光雷达数据获取
#include <sys/stat.h>          // 文件状态信息和目录操作

#include <sys/stat.h>          // 用于 stat() 和 mkdir() 函数
#include <fstream>             // 用于文件流操作
#include <mutex>               // 用于互斥锁，保护共享数据
#include <memory>              // 用于 std::unique_ptr 智能指针
#include <iomanip>             // 用于 std::put_time，格式化时间输出

// 定义存储两个点的结构体，可能用于岸线检测或其他边界点标记
struct two_points
{
    cv::Point2f largesty1;     // 最大y值点
    cv::Point2f largesty2;     // 第二大y值点
    // 默认构造函数
    two_points() : largesty1(0, 0), largesty2(0, 0) {}
};

/**
 * WorkFlow类 - 系统核心工作流程控制器
 */
class WorkFlow
{
private:
    Config config_;            // 系统配置
    bool shouldStop_ = false;  // 停止标志位
    std::thread encoding_thread_; // 编码线程

    RGAOperations rgaoperations_; // RGA图像处理操作

    BlendType m_blendType;     // 融合类型枚举

    // 图像尺寸参数
    int vicImgWidth_;          // 可见光图像宽度
    int vicImgHeight_;         // 可见光图像高度
    int irImgWidth_;           // 红外图像宽度
    int irImgHeight_;          // 红外图像高度

    // 控制参数成员变量
    uint8_t selftest = 0x00;         // 自检状态
    uint8_t display_mode = 0x00;     // 显示模式
    uint8_t fusion_format = 0x00;    // 融合格式
    uint8_t stabilization_mode = 0x00; // 稳定模式
    uint8_t encode_rate = 0x00;      // 编码率
    uint8_t reboot = 0x00;           // 重启标志
    uint8_t ir_control_code = 0x00;  // 红外控制码
    uint8_t vic_control_code = 0x00; // 可见光控制码
    uint8_t log_save = 0x00;         // 日志保存标志
    uint8_t ota_upgrade_display = 0x00; // OTA升级显示
    uint8_t ota_upgrade_start = 0x00;   // OTA升级开始
    uint8_t ota_version = 0x00;      // OTA版本
    uint16_t focus_area_low = 0x00;  // 聚焦区域低边界
    uint16_t focus_area_high = 0x00; // 聚焦区域高边界
    uint8_t char_overlay = 0x01;     // 字符叠加标志
    uint8_t task_mode = 0x00;        // 任务模式
    uint8_t lidar_status = 0x00;     // 激光雷达状态

    uint8_t detection_format = 0x01; // 检测格式
    // uint8_t detection_mode = 0x00; // 检测模式(已注释)

    NetworkManager networkManager_; // 网络管理器实例

    bool need_flip_horizontally = false; // 是否需要水平翻转图像

    // 图像校正和映射矩阵
    cv::Mat vic_map1_, vic_map2_;     // 可见光相机畸变校正映射
    cv::Mat ir_map1_, ir_map2_;       // 红外相机畸变校正映射
    cv::Mat map_x_, map_y_;           // 图像映射矩阵
    cv::Mat combined_map_x_, combined_map_y_; // 组合映射矩阵
    cv::Vec3b ir2ColorMap[256];       // 红外伪彩色映射表
    cv::Vec3b dist2ColorMap[256];     // 距离伪彩色映射表

    // 私有方法
    void processFormat(const uint8_t *control_frame, size_t frame_length); // 处理格式控制帧
    void initializeCalibrationAndColorMaps(); // 初始化校正和颜色映射
    void initializeVideoPipeline();  // 初始化视频管道
    // LidarManualSelection lidar_selection; // 激光雷达手动选择(已注释)
    void updateBlendType(uint8_t fusion_format, BlendType &m_blendType); // 更新混合类型
    void updateEncodeRate(uint8_t encode_rate); // 更新编码率

    // GStreamer编码器管道组件
    GstPipeline *g_enc_pipeline;    // 编码管道
    GstElement *g_enc_src;          // 编码源
    GstElement *g_enc_filter;       // 编码过滤器
    GstElement *g_encoder;          // 编码器元素
    GstElement *g_parse;            // 解析器
    GstElement *g_stream_sink;      // 流媒体接收器
    unsigned char *g_encData;       // 编码数据缓冲区
    bool createEncoderPipeline(int bpsRate); // 创建编码器管道
    void setEncoderBps(int bpsRate); // 设置编码器比特率
    static GstFlowReturn mediaStreamProc(GstElement *sink, GstBuffer *buffer, GstPad *pad, gpointer data); // 媒体流处理回调

    // GStreamer视频管道组件
    GstPipeline *g_pipeline;       // 视频管道
    GstElement *g_src;             // 视频源
    GstElement *g_video_filter;    // 视频过滤器
    GstElement *g_video_sink;      // 视频接收器

    GstElement *g_scale;           // 缩放元素

    // 视频源管理方法
    bool createVisibleVideoSource(int width, int height, const char *device, GstAppSinkCallbacks callbacks); // 创建可见光视频源
    bool initializeIRSource(const char *device); // 初始化红外源
    bool initializeVisibleSource(const char *device); // 初始化可见光源
    static GstFlowReturn irSinkCallback(GstAppSink *appsink, gpointer user_data); // 红外接收回调
    static GstFlowReturn vicSinkCallback(GstAppSink *appsink, gpointer user_data); // 可见光接收回调
    static GstSample *pullSample(GstAppSink *appsink); // 拉取样本
    static bool mapBuffer(GstBuffer *buffer, GstMapInfo &mapinfo); // 映射缓冲区
    static void getCapsInfo(GstAppSink *appsink, int &width, int &height); // 获取能力信息

    const size_t max_queue_size_ = 1; // 最大队列大小

    // 线程安全的图像缓冲区
    std::mutex aligned_ir_for_fusion_mutex_; // 用于红外图像融合的互斥锁
    cv::Mat aligned_ir_for_fusion;           // 对齐的红外图像

    std::mutex aligned_lidar_for_fusion_mutex_; // 用于激光雷达图像融合的互斥锁
    cv::Mat aligned_lidar_for_fusion;           // 对齐的激光雷达图像

    std::mutex stream_image_mutex_;           // 流图像互斥锁
    cv::Mat stream_image;                     // 流图像缓冲区

    std::mutex detection_image_mutex_;        // 检测图像互斥锁
    cv::Mat detection_image;                  // 检测图像缓冲区

    void encodingAndStreamingThread(); // 编码和流媒体线程函数
    void pushBufferToEncoder(const unsigned char *data, int width, int height); // 将缓冲区推送到编码器

    // 目标检测线程
    std::thread detection_thread_;                // 检测线程
    std::atomic<bool> detection_thread_running_{false}; // 检测线程运行标志
    void startDetectionThread();                  // 启动检测线程
    void detectionThreadFunction();               // 检测线程函数
    void stopDetectionThread();                   // 停止检测线程

    // 岸线检测线程
    std::thread shoreline_detection_thread_;      // 岸线检测线程
    std::atomic<bool> shoreline_detection_running_{false}; // 岸线检测运行标志

    // 检测相关变量
    std::mutex detection_input_image_mutex;       // 检测输入图像互斥锁
    cv::Mat detection_input_image;                // 检测输入图像
    Detector detector_;                           // 检测器实例
    detect_result_group_t detect_results;         // 检测结果组

    // 激光雷达相关变量
    std::thread lidar_thread_;                   // 激光雷达线程
    std::atomic<bool> lidar_thread_running_;     // 激光雷达线程运行标志
    GetLidarData *m_GetLidarData = new GetLidarData_CX128S2; // 激光雷达数据获取实例

    LidarStateParam mLidarStateParam;            // 激光雷达状态参数
    std::string mInfo1;                          // 激光雷达信息
    void lidarThreadFunction();                  // 激光雷达线程函数
    void stopLidarThread();                      // 停止激光雷达线程
    void processLidarData();                     // 处理激光雷达数据
    double fx, fy, cx, cy;                       // 相机内参
    cv::Mat extrinsicMatrix;                     // 外部矩阵（外参）
    std::vector<MuchLidarData> m_LidarData;      // 点云数据存储向量

    void prepareStatusFrameThread();              // 准备状态帧线程
    uint8_t g_status_frame[138];                  // 状态帧缓冲区

    // 激光雷达手动选择模式枚举
    enum class LidarManualSelectionMode : uint8_t
    {
        NO_ACTION = 0x0,    // 无动作
        ACTIVATE = 0x1,     // 激活
        DEACTIVATE = 0x2,   // 停用
    };

    // 激光雷达手动选择尺寸枚举
    enum class LidarManualSelectionSize : uint8_t
    {
        S16 = 0x0,          // 16 x 16 尺寸
        S32 = 0x1,          // 32 x 32 尺寸
        S64 = 0x2,          // 64 x 64 尺寸
    };

    // 激光雷达手动选择控制结构体
    struct LidarManualSelectionControl
    {
        LidarManualSelectionMode mode; // 模式
        uint16_t x;                    // X坐标
        uint16_t y;                    // Y坐标
        LidarManualSelectionSize size; // 尺寸
    };

    // 激光雷达手动选择上下文结构体
    struct LidarManualSelectionContext
    {
        LidarManualSelectionControl ctrl; // 控制信息
        bool empty = true;                // 是否为空
        struct                            // ROI区域信息
        {
            cv::Rect rect;                // 矩形区域
            double ave_distance;          // 平均距离
            double pitch;                 // 俯仰角
            double azimuth;               // 方位角
        } roi;
        struct                            // 格式化文本信息
        {
            struct                        // 文本样式
            {
                double scale = 0.9;                  // 比例
                int face = cv::FONT_HERSHEY_SIMPLEX; // 字体
                int thickness = 2;                   // 厚度
                cv::Scalar color = cv::Scalar(0, 0, 255); // 颜色(红色)
            } style;
            std::string label;            // 标签文本
            cv::Size size;                // 尺寸
            int baseline = 0;             // 基线
        } formatted_txt;
    } lidar_manual_selection_ctx;         // 激光雷达手动选择上下文实例

    // 获取激光雷达模式字符串
    std::string getLidarModeString(LidarManualSelectionMode mode)
    {
        switch (mode)
        {
        case LidarManualSelectionMode::NO_ACTION:
            return "NO_ACTION";           // 无动作
        case LidarManualSelectionMode::ACTIVATE:
            return "ACTIVATE";            // 激活
        case LidarManualSelectionMode::DEACTIVATE:
            return "DEACTIVATE";          // 停用
        default:
            return "UNKNOWN";             // 未知
        }
    }

    // 打印激光雷达模式
    void printLidarMode(LidarManualSelectionMode mode)
    {
        std::cout << "Mode: " << getLidarModeString(mode)
                  << " (Value: 0x" << std::hex
                  << static_cast<int>(mode) << ")"
                  << std::endl;
    }

    void calcLidarManualSelection(cv::Mat &mat_distance, LidarManualSelectionContext &selection); // 计算激光雷达手动选择
    void drawLidarManualSelection(cv::Mat &mat_image, const LidarManualSelectionContext &selection); // 绘制激光雷达手动选择

    // 图像捕获线程
    std::thread image_capture_thread_;                  // 图像捕获线程
    std::atomic<bool> image_capture_thread_running_{false}; // 图像捕获线程运行标志
    std::string log_picture_folder_;                    // 图片日志文件夹

    RGAOperations rgaOps;                              // RGA操作实例(可能是冗余的)

    size_t frame_counter_ = 0;                         // 帧计数器
    const size_t MAX_FRAMES_TO_SAVE = 10;              // 最大保存帧数

    std::mutex distance_image_for_fusion_mutex_;       // 用于融合的距离图像互斥锁
    cv::Mat distance_image;                            // 距离图像

    double THRESHOLD_FACTOR = 0.53;                    // 阈值因子

    two_points current_points_;                        // 当前点
    void fixPoints(two_points &points);                // 修正点位置

    cv::Point2f left_bottom;                           // 左下角点
    cv::Point2f right_bottom;                          // 右下角点

    bool receiveBoxCoordinates(int socket_fd, struct sockaddr_in &sender_addr); // 接收盒坐标
    void startReceiving();                             // 开始接收

    int outputImgWidth_;                               // 输出视频宽度
    int outputImgHeight_;                              // 输出视频高度

    void setOutputDimensions();                        // 设置输出尺寸
    bool updateResolution(int new_width, int new_height); // 更新分辨率

    bool validateDisplayModeAndIP(uint8_t display_mode, const std::string &localIP); // 验证显示模式和IP

    bool single_mode;                                  // 单一模式标志

    // 文件操作相关
    std::mutex file_mutex_;                            // 文件互斥锁
    std::unique_ptr<std::ofstream> h264File_;          // H.264文件指针
    std::string currentFilePath_;                      // 当前文件路径
    bool fileInitialized_ = false;                     // 文件初始化标志
    const std::string outputDir_ = "recorded_video";   // 输出目录

    bool irNeedForDetection = false;                   // 是否需要红外用于检测



    // 定义线程类型枚举
    enum class ThreadType {
        VISIBLE_CALLBACK,    // 可见光回调线程
        IR_CALLBACK,         // 红外回调线程
        ENCODING_STREAMING,  // 编码和流媒体线程
        DETECTION,           // 检测线程
        LIDAR,               // 激光雷达线程
        // NETWORK_CONTROL,     // 网络控制线程
        // NETWORK_STATUS       // 网络状态线程
    };

    // 线程状态监控结构 - 修复std::atomic的拷贝问题
    struct ThreadMonitorInfo {
        uint64_t callback_counter;               // 非原子计数器，用于在监控线程中比较
        std::atomic<uint64_t> atomic_counter;    // 原子计数器，用于线程安全更新
        std::atomic<uint64_t> last_timestamp;    // 最后活动时间戳
        uint64_t timeout_seconds;                // 超时时间（秒）
        std::function<bool()> reinitialize_function;   // 重新初始化函数
        std::string thread_name;                 // 线程名称
        
        ThreadMonitorInfo() : 
            callback_counter(0), 
            atomic_counter(0), 
            last_timestamp(0),
            timeout_seconds(0) {}
    };



    // 线程监控相关
    std::atomic<bool> monitor_thread_running_;                            // 监控线程运行标志
    std::thread monitor_thread_;                                          // 监控线程
    std::map<ThreadType, std::shared_ptr<ThreadMonitorInfo>> thread_monitors;  // 使用shared_ptr避免拷贝问题
    std::mutex thread_monitors_mutex_;                                    // 线程监控映射互斥锁
    
    void startMonitorThread();                                            // 启动监控线程
    void stopMonitorThread();                                             // 停止监控线程
    void monitorThreadFunction();                                         // 监控线程函数
    void registerThreadMonitor(ThreadType type,                           // 注册线程监控
                              const std::string& name,
                              uint64_t timeout_seconds,
                              std::function<bool()> reinitialize_function);
    void updateThreadTimestamp(ThreadType type);                          // 更新线程时间戳
    bool reinitializeVisibleSource();                                     // 重新初始化可见光源
    bool reinitializeIRSource();                                          // 重新初始化红外源
    bool reinitializeEncodingStreaming();                                 // 重新初始化编码和流媒体
    bool reinitializeDetection();                                         // 重新初始化检测
    bool reinitializeLidar();                                             // 重新初始化激光雷达
    bool reinitializeNetworkControl();                                    // 重新初始化网络控制
    bool reinitializeNetworkStatus();                                     // 重新初始化网络状态

    void savePointCloudCSV(const std::vector<MuchLidarData> &lidar_data, const std::string &filename);

public:
    WorkFlow(const Config &config);                    // 构造函数
    ~WorkFlow();                                       // 析构函数

    void initialize();                                 // 初始化方法
    void run();                                        // 运行方法
    void stop();                                       // 停止方法
};

#endif // WORKFLOW_H  // 防止头文件重复包含的宏定义结束