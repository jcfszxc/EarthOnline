// ===== Config类头文件 =====
// 本文件定义了用于加载和管理系统配置的Config类，包括网络设置、摄像头参数、
// 检测模型配置、激光雷达设置以及调试选项等各种系统参数

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <json.hpp>           // 用于JSON解析
#include <opencv2/opencv.hpp> // OpenCV库
#include "Logger.h"           // 日志记录功能
#include "Utils.h"            // 工具函数

#include <fcntl.h>   // 用于open()、O_RDWR、O_NOCTTY、O_NDELAY函数和常量
#include <termios.h> // 用于termios结构体、tcgetattr()、tcsetattr()函数
#include <unistd.h>  // 用于tcflush()、TCIFLUSH函数和常量

#include <sys/types.h>  // 系统类型定义
#include <sys/socket.h> // 套接字相关函数
#include <netdb.h>      // 网络数据库操作
#include <ifaddrs.h>    // 接口地址结构体
#include <arpa/inet.h>  // 互联网地址转换函数
#include <bitset>       // 用于位操作

class Config
{
public:
    // 构造函数，从指定文件名加载配置
    Config(const std::string &filename);

    // ===== 基本配置项获取函数 =====
    // 以下函数从配置中获取基本数据类型，提供默认值作为备选
    std::string getString(const std::string &key, const std::string &defaultValue) const;
    int getInt(const std::string &key, int defaultValue) const;
    double getDouble(const std::string &key, double defaultValue) const;
    bool getBool(const std::string &key, bool defaultValue) const;

    // ===== 网络相关配置获取函数 =====
    std::string getPjIpAddress() const; // 获取投影仪IP地址
    int getReceivePort() const;         // 获取接收端口
    int getSendPort() const;            // 获取发送端口

    // ===== 调试选项获取函数 =====
    bool getDebugOption(const std::string &option) const; // 获取特定调试选项的值

    // ===== 摄像头配置获取函数 =====
    std::string getVisibleCameraSerialPort() const;  // 获取可见光摄像头串口
    std::string getInfraredCameraSerialPort() const; // 获取红外摄像头串口
    cv::Mat getVicUndistortionK() const;             // 获取可见光相机内参矩阵
    cv::Mat getVicUndistortionD() const;             // 获取可见光相机畸变系数
    cv::Mat getIrUndistortionK() const;              // 获取红外相机内参矩阵
    cv::Mat getIrUndistortionD() const;              // 获取红外相机畸变系数
    cv::Mat getRegistrationH() const;                // 获取配准变换矩阵
    std::string getIRMappingMethod() const;          // 获取红外映射方法

    // 获取指定摄像头的图像尺寸
    int getImageWidth(const std::string &camera) const;  // 获取图像宽度
    int getImageHeight(const std::string &camera) const; // 获取图像高度
    int getInitialBpsRate() const;                       // 获取初始波特率

    // ===== 设备路径和类型获取函数 =====
    std::string getIRVideoDevice() const;      // 获取红外视频设备路径
    std::string getVisibleVideoDevice() const; // 获取可见光视频设备路径
    std::string getNetInterfaceName() const;   // 获取网络接口名称
    BlendType getDefaultBlendType() const;     // 获取默认混合类型

    // ===== 模型配置获取函数 =====
    std::string getRKNNModelPath() const;    // 获取RKNN模型路径
    std::string getLabelNameTxtPath() const; // 获取标签名称文本路径
    float getBoxThresh() const;              // 获取边界框阈值
    float getNmsThresh() const;              // 获取非极大值抑制阈值
    int getObjClassNum() const;              // 获取对象类别数量

    // ===== 激光雷达配置获取函数 =====
    int getLidarPortDat() const;    // 获取激光雷达数据端口
    int getLidarPortDev() const;    // 获取激光雷达设备端口
    std::string getLidarIP() const; // 获取激光雷达IP地址

    // ===== 网络通信配置获取函数 =====
    std::string getLocalIP() const; // 获取本地IP地址
    int getUDPSendPort() const;     // 获取UDP发送端口

    // ===== 激光雷达内外参配置 =====
    // 激光雷达内参结构体定义
    struct LidarIntrinsic
    {
        double fx, fy, cx, cy; // 内参矩阵参数：焦距和主点坐标
    };
    LidarIntrinsic lidar_intrinsic_; // 激光雷达内参实例
    cv::Mat lidar_extrinsic_;        // 激光雷达外参矩阵
    void loadLidarParameters();      // 加载激光雷达参数

    // 获取激光雷达相关参数
    LidarIntrinsic getLidarIntrinsic() const { return lidar_intrinsic_; } // 获取激光雷达内参
    cv::Mat getLidarExtrinsic() const { return lidar_extrinsic_; }        // 获取激光雷达外参
    bool getLidarThreadEnabled() const;                                   // 获取激光雷达线程是否启用

    // ===== 调试图像保存和RTSP配置 =====
    bool getSaveImages() const;                           // 是否保存图像
    int getSaveInterval() const;                          // 图像保存间隔
    bool getRTSPEnabled() const { return rtsp_enabled_; } // 是否启用RTSP流

    // ===== 调试显示和状态帧配置 =====
    bool getDrawBoundingBoxesLocally() const;                        // 是否本地绘制边界框
    bool getDrawShoreLineLocally() const;                            // 是否本地绘制岸线
    std::map<int, uint8_t> getStatusFrameDebugModifications() const; // 获取状态帧调试修改
    bool getStrictDetectionMode() const;                             // 是否使用严格检测模式

    // ===== 偏移和偏差参数获取函数 =====
    int getOffset() const { return offset_; }                 // 获取偏移量
    double getDistanceBias() const { return distance_bias_; } // 获取距离偏差
    double getTargetBias() const { return target_id_bias_; }  // 获取目标ID偏差

    // ===== 输出图像参数获取函数 =====
    int getOutWidth() const;  // 获取输出图像宽度
    int getOutHeight() const; // 获取输出图像高度

    // ===== 模型特性获取函数 =====
    bool isQuantizedModel() const; // 判断是否为量化模型

    // ===== 中心点范围参数获取函数 =====
    float getCenterYMin() const; // 获取中心Y坐标最小值
    float getCenterYMax() const; // 获取中心Y坐标最大值

    // 获取多边形过滤区域的顶点
    const std::vector<std::pair<int, int>>& getFilterPolygon() const;


    
    // UDP发送参数获取函数
    int getUDPPacketInterval() const;
    int getUDPSleepDuration() const;
    
    // UDP发送参数设置函数（可选）
    void setUDPPacketInterval(int interval);
    void setUDPSleepDuration(int duration);

private:
    nlohmann::json config_;                   // 存储配置的JSON对象
    std::string pj_ip_address_;               // 投影仪IP地址
    int receive_port_;                        // 接收端口号
    int send_port_;                           // 发送端口号
    nlohmann::json debug_options_;            // 调试选项JSON对象
    std::string visible_camera_serial_port_;  // 可见光摄像头串口
    std::string infrared_camera_serial_port_; // 红外摄像头串口
    cv::Mat K_vic;                            // 可见光相机内参矩阵
    cv::Mat K_ir;                             // 红外相机内参矩阵
    cv::Mat D_vic;                            // 可见光相机畸变系数
    cv::Mat D_ir;                             // 红外相机畸变系数
    cv::Mat H;                                // 配准变换矩阵
    std::string ir_mapping_method_;           // 红外映射方法
    std::string net_interface_name_;          // 网络接口名称
    BlendType default_blend_type_;            // 默认混合类型
    std::string rknn_model_path_;             // RKNN模型路径

    std::string label_name_txt_path_; // 标签名称文本路径
    float box_thresh_;                // 边界框阈值
    float nms_thresh_;                // 非极大值抑制阈值
    int obj_class_num_;               // 对象类别数量

    void loadCalibrationData(); // 加载标定数据的私有方法

    int lidar_port_dat_;        // 激光雷达数据端口
    int lidar_port_dev_;        // 激光雷达设备端口
    bool lidar_thread_enabled_; // 激光雷达线程是否启用
    std::string lidar_ip_;      // 激光雷达IP地址

    std::string local_ip_;          // 本地IP地址
    int udp_send_port_;             // UDP发送端口
    void determineLocalIPAndPort(); // 确定本地IP和端口的私有方法
    void loadDebugOptions();        // 加载调试选项的私有方法
    bool rtsp_enabled_;             // 是否启用RTSP流
    bool udp_enabled_;             // 是否启用RTSP流

    bool draw_bounding_boxes_locally_; // 是否本地绘制边界框
    bool draw_shore_line_locally_;     // 是否本地绘制岸线

    bool strict_detection_mode_; // 是否使用严格检测模式

    int offset_;                  // 偏移量
    double distance_bias_ = 0.0;  // 距离偏差，默认值为0
    double target_id_bias_ = 0.0; // 目标ID偏差，默认值为0

    int outputImgWidth_;  // 输出视频宽度
    int outputImgHeight_; // 输出视频高度

    bool is_quantized_model_; // 是否为量化RKNN模型

    float center_y_min_ = 350.0; // 中心Y坐标最小值，默认值350.0
    float center_y_max_ = 800.0; // 中心Y坐标最大值，默认值800.0

    // 多边形过滤区域的顶点
    std::vector<std::pair<int, int>> filter_polygon_;
    
    void loadFilterPolygon();


    
    // 新增UDP发送控制参数
    int udp_packet_interval_ = 40;    // 每发送多少个数据包后休眠（默认40）
    int udp_sleep_duration_ms_ = 1;   // 休眠时间，单位毫秒（默认1毫秒）
    
    // 加载UDP发送参数的私有函数
    void loadUDPSendParameters();

};

#endif // CONFIG_H
