/**
 * Config.cpp - 系统配置管理类的实现
 *
 * 本文件实现了Config类，负责从JSON配置文件中读取和管理系统的各种配置参数，
 * 包括网络设置、相机参数、LiDAR设置、校准数据、调试选项等。Config类为系统
 * 的其他组件提供配置信息的访问接口。
 */

#include "Config.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

/**
 * 构造函数，从指定的配置文件初始化Config对象
 * @param filename 配置文件路径
 */
Config::Config(const std::string &filename)
{
    Logger::info("Initializing Config with file: " + filename);
    // 打开并解析配置文件
    std::ifstream file(filename);
    if (!file.is_open())
    {
        Logger::error("Unable to open config file: " + filename);
        throw std::runtime_error("Unable to open config file: " + filename);
    }
    config_ = nlohmann::json::parse(file);
    Logger::debug("Config file parsed successfully");

    // 读取网络设置部分
    if (config_.contains("network_settings") && config_["network_settings"].is_object())
    {
        auto &network = config_["network_settings"];

        // 获取IP地址和端口参数
        pj_ip_address_ = network.value("pj_ip_address", "");
        receive_port_ = network.value("receive_port", 5000);
        send_port_ = network.value("send_port", 5100);

        // 获取RTSP启用状态
        rtsp_enabled_ = network.value("rtsp_enabled", false);
        udp_enabled_ = network.value("udp_enabled", true);  // 默认启用UDP保持向后兼容

        Logger::info("Network settings loaded: IP=" + pj_ip_address_ +
                    ", Receive Port=" + std::to_string(receive_port_) +
                    ", Send Port=" + std::to_string(send_port_) +
                    ", UDP Enabled=" + (udp_enabled_ ? "true" : "false") +
                    ", RTSP Enabled=" + (rtsp_enabled_ ? "true" : "false"));
    }
    else
    {
        Logger::error("network_settings not found in config file or is not an object");
        throw std::runtime_error("network_settings not found in config file or is not an object.");
    }

    // 读取调试选项
    if (config_.contains("DEBUG_OPTIONS") && config_["DEBUG_OPTIONS"].is_object())
    {
        debug_options_ = config_["DEBUG_OPTIONS"];
        Logger::debug("Debug options loaded");
    }
    else
    {
        Logger::warning("DEBUG_OPTIONS not found in config file or is not an object");
    }

    // 读取可见光相机串口设置
    if (config_.contains("visible_camera_settings") && config_["visible_camera_settings"].is_object())
    {
        auto &camera_settings = config_["visible_camera_settings"];
        visible_camera_serial_port_ = camera_settings.value("serial_port", "/dev/ttyS4");
        Logger::info("Visible camera serial port: " + visible_camera_serial_port_);
    }
    else
    {
        Logger::warning("visible_camera_settings not found in config file or is not an object. Using default serial port");
        visible_camera_serial_port_ = "/dev/ttyS4"; // 使用默认串口
    }

    // 读取红外相机串口设置
    if (config_.contains("infrared_camera_settings") && config_["infrared_camera_settings"].is_object())
    {
        auto &camera_settings = config_["infrared_camera_settings"];
        infrared_camera_serial_port_ = camera_settings.value("serial_port", "/dev/ttyS7");
        Logger::info("Infrared camera serial port: " + infrared_camera_serial_port_);
    }
    else
    {
        Logger::warning("infrared_camera_settings not found in config file or is not an object. Using default serial port");
        infrared_camera_serial_port_ = "/dev/ttyS7"; // 使用默认串口
    }

    // 加载相机校准数据
    loadCalibrationData();

    // 加载LiDAR参数
    loadLidarParameters();

    // // 加载红外映射方法设置
    // if (config_.contains("ir_settings") && config_["ir_settings"].is_object())
    // {
    //     auto &ir_settings = config_["ir_settings"];
    //     ir_mapping_method_ = ir_settings.value("mapping_method", "greenspace");
    //     Logger::info("IR mapping method: " + ir_mapping_method_);
    // }
    // else
    // {
    //     Logger::warning("ir_settings not found in config file or is not an object. Using default mapping method: greenspace");
    //     ir_mapping_method_ = "greenspace"; // 使用默认映射方法
    // }

    // 加载红外映射方法设置
    if (config_.contains("ir_settings") && config_["ir_settings"].is_object())
    {
        auto &ir_settings = config_["ir_settings"];
        ir_mapping_method_ = ir_settings.value("mapping_method", "greenspace");
        Logger::info("IR mapping method: " + ir_mapping_method_);
        
        // 验证映射方法
        if (ir_mapping_method_ != "pseudocolor")
        {
            Logger::error("Pseudo-color transformation failed");
        }
    }
    else
    {
        Logger::warning("ir_settings not found in config file or is not an object. Using default mapping method: greenspace");
        ir_mapping_method_ = "greenspace"; // 使用默认映射方法
        Logger::error("Pseudo-color transformation failed");
    }    

    // 读取网络接口名称
    if (config_.contains("network_settings") && config_["network_settings"].is_object())
    {
        auto &network = config_["network_settings"];
        net_interface_name_ = network.value("interface_name", "eth0"); // 默认为 "eth0"
        Logger::info("Network interface name: " + net_interface_name_);
    }
    else
    {
        Logger::warning("network_settings not found or is not an object. Using default interface name: eth0");
        net_interface_name_ = "eth0";
    }

    // 设置默认混合类型
    if (config_.contains("default_blend_type"))
    {
        std::string blend_type_str = config_["default_blend_type"].get<std::string>();
        // 根据字符串配置设置对应的混合类型枚举值
        if (blend_type_str == "V")
            default_blend_type_ = BlendType::V; // 只显示可见光
        else if (blend_type_str == "I")
            default_blend_type_ = BlendType::I; // 只显示红外
        else if (blend_type_str == "VL")
            default_blend_type_ = BlendType::VL; // 可见光+LiDAR
        else if (blend_type_str == "IL")
            default_blend_type_ = BlendType::IL; // 红外+LiDAR
        else if (blend_type_str == "VI")
            default_blend_type_ = BlendType::VI; // 可见光+红外
        else if (blend_type_str == "VIL")
            default_blend_type_ = BlendType::VIL; // 可见光+红外+LiDAR
        else if (blend_type_str == "L")
            default_blend_type_ = BlendType::L; // 只显示LiDAR
        else
        {
            Logger::warning("Invalid default_blend_type in config, using V as default");
            default_blend_type_ = BlendType::V; // 默认为只显示可见光
        }
        Logger::info("Default blend type set to: " + blend_type_str);
    }
    else
    {
        Logger::warning("default_blend_type not found in config, using V as default");
        default_blend_type_ = BlendType::V; // 配置文件中未找到时使用默认值
    }

    // 读取RKNN神经网络模型设置
    if (config_.contains("rknn_settings") && config_["rknn_settings"].is_object())
    {
        auto &rknn_settings = config_["rknn_settings"];
        rknn_model_path_ = rknn_settings.value("model_path", "");
        is_quantized_model_ = rknn_settings.value("is_quantized", true); // 默认为量化模型

        if (!rknn_model_path_.empty())
        {
            Logger::info("RKNN model path loaded: " + rknn_model_path_);
        }
        else
        {
            Logger::warning("RKNN model path is empty in config file");
        }

        Logger::info("RKNN model is " + std::string(is_quantized_model_ ? "quantized" : "not quantized"));
    }
    else
    {
        Logger::warning("rknn_settings not found in config file or is not an object");
        is_quantized_model_ = false; // 设置默认值
    }

    // 读取YOLO目标检测相关设置
    if (config_.contains("yolo_settings") && config_["yolo_settings"].is_object())
    {
        auto &yolo = config_["yolo_settings"];
        label_name_txt_path_ = yolo.value("label_name_txt_path", "./models/labels_list.txt");
        box_thresh_ = yolo.value("box_thresh", 0.25f);   // 边界框阈值
        nms_thresh_ = yolo.value("nms_thresh", 0.45f);   // 非极大值抑制阈值
        obj_class_num_ = yolo.value("obj_class_num", 1); // 对象类别数量

        Logger::info("YOLO settings loaded: Label path=" + label_name_txt_path_ +
                     ", Box Thresh=" + std::to_string(box_thresh_) +
                     ", NMS Thresh=" + std::to_string(nms_thresh_) +
                     ", Object Class Num=" + std::to_string(obj_class_num_));
    }
    else
    {
        Logger::warning("yolo_settings not found in config file or is not an object. Using default values.");
        // 使用默认值
        label_name_txt_path_ = "./models/labels_list.txt";
        box_thresh_ = 0.25f;
        nms_thresh_ = 0.45f;
        obj_class_num_ = 1;
    }

    // 读取LiDAR雷达设置
    if (config_.contains("lidar_settings") && config_["lidar_settings"].is_object())
    {
        auto &lidar = config_["lidar_settings"];
        lidar_port_dat_ = lidar.value("port_dat", 2368);      // 数据端口
        lidar_port_dev_ = lidar.value("port_dev", 2369);      // 设备端口
        lidar_ip_ = lidar.value("lidar_ip", "192.168.1.200"); // LiDAR IP地址
        Logger::info("Lidar settings loaded: DAT Port=" + std::to_string(lidar_port_dat_) +
                     ", DEV Port=" + std::to_string(lidar_port_dev_) +
                     ", Lidar IP=" + lidar_ip_);
    }
    else
    {
        Logger::warning("lidar_settings not found in config file or is not an object. Using default values.");
        // 使用默认值
        lidar_port_dat_ = 2368;
        lidar_port_dev_ = 2369;
        lidar_ip_ = "192.168.1.200";
    }

    // 读取相机距离偏差和目标ID偏差
    if (config_.contains("camera_settings") && config_["camera_settings"].is_object())
    {
        auto &camera_settings = config_["camera_settings"];
        // 读取距离偏差
        distance_bias_ = camera_settings.value("distance_bias", 0.0);
        Logger::info("Distance bias loaded: " + std::to_string(distance_bias_));

        // 读取目标ID偏差
        if (camera_settings.contains("target_id_bias") && camera_settings["target_id_bias"].is_number())
        {
            target_id_bias_ = camera_settings["target_id_bias"].get<double>();
            Logger::info("Target ID bias loaded: " + std::to_string(target_id_bias_));
        }
        else
        {
            Logger::warning("target_id_bias not found in camera_settings or is not a number. Using default target ID bias: 0.0");
            target_id_bias_ = 0.0;
        }
    }
    else
    {
        Logger::warning("camera_settings not found in config file or is not an object. Using default distance bias: 0.0");
        distance_bias_ = 0.0;
        target_id_bias_ = 0.0; // 设置目标ID偏差的默认值
    }

    // 确定本地IP地址和端口
    determineLocalIPAndPort();

    // 读取检测阈值参数
    if (config_.contains("detection_thresholds") && config_["detection_thresholds"].is_object())
    {
        auto &thresholds = config_["detection_thresholds"];
        center_y_min_ = thresholds.value("center_y_min", 324.0); // Y坐标最小值
        center_y_max_ = thresholds.value("center_y_max", 864.0); // Y坐标最大值
        Logger::info("Detection thresholds loaded: center_y_min=" + std::to_string(center_y_min_) +
                     ", center_y_max=" + std::to_string(center_y_max_));
    }
    else
    {
        Logger::warning("detection_thresholds not found in config file or is not an object. Using default values: center_y_min=324.0, center_y_max=864.0");
        center_y_min_ = 350.0;
        center_y_max_ = 800.0;
    }

    loadFilterPolygon();

    // 加载调试选项
    loadDebugOptions();

    Logger::info("Config initialization completed");
}

/**
 * 加载相机校准数据
 * 从配置中读取相机内参、畸变校正参数和配准矩阵
 */
void Config::loadCalibrationData()
{
    Logger::debug("Loading calibration data");
    if (config_.contains("calibration") && config_["calibration"].is_object())
    {
        auto &calibration = config_["calibration"];

        // 加载可见光相机校准数据
        if (calibration.contains("vic") && calibration["vic"].is_object())
        {
            auto &vic = calibration["vic"];
            // 加载内参矩阵K
            if (vic.contains("undistortion_k") && vic["undistortion_k"].is_array())
            {
                K_vic = cv::Mat(3, 3, CV_64F);
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j)
                        K_vic.at<double>(i, j) = vic["undistortion_k"][i][j];
                Logger::debug("VIC undistortion_k loaded");
            }
            // 加载畸变系数D
            if (vic.contains("undistortion_d") && vic["undistortion_d"].is_array())
            {
                D_vic = cv::Mat(1, 5, CV_64F);
                for (int i = 0; i < 5; ++i)
                    D_vic.at<double>(0, i) = vic["undistortion_d"][0][i];
                Logger::debug("VIC undistortion_d loaded");
            }
        }
        else
        {
            Logger::warning("VIC calibration data not found or incomplete");
        }

        // 加载红外相机校准数据
        if (calibration.contains("ir") && calibration["ir"].is_object())
        {
            auto &ir = calibration["ir"];
            // 加载内参矩阵K
            if (ir.contains("undistortion_k") && ir["undistortion_k"].is_array())
            {
                K_ir = cv::Mat(3, 3, CV_64F);
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j)
                        K_ir.at<double>(i, j) = ir["undistortion_k"][i][j];
                Logger::debug("IR undistortion_k loaded");
            }
            // 加载畸变系数D
            if (ir.contains("undistortion_d") && ir["undistortion_d"].is_array())
            {
                D_ir = cv::Mat(1, 5, CV_64F);
                for (int i = 0; i < 5; ++i)
                    D_ir.at<double>(0, i) = ir["undistortion_d"][0][i];
                Logger::debug("IR undistortion_d loaded");
            }
            // 加载两个相机之间的单应性变换矩阵H
            if (ir.contains("registration_h") && ir["registration_h"].is_array())
            {
                H = cv::Mat(3, 3, CV_64F);
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j)
                        H.at<double>(i, j) = ir["registration_h"][i][j];
                Logger::debug("IR registration_h loaded");
            }
        }
        else
        {
            Logger::warning("IR calibration data not found or incomplete");
        }

        // 读取输出分辨率设置
        if (config_.contains("output_dimensions") && config_["output_dimensions"].is_object())
        {
            auto &output = config_["output_dimensions"];
            outputImgWidth_ = output.value("width", 1920);   // 默认宽度 1920
            outputImgHeight_ = output.value("height", 1080); // 默认高度 1080
            Logger::info("Output dimensions loaded: " + std::to_string(outputImgWidth_) + "x" +
                         std::to_string(outputImgHeight_));
        }
        else
        {
            Logger::warning("Output dimensions not found in config, using defaults 1920x1080");
            outputImgWidth_ = 1920;
            outputImgHeight_ = 1080;
        }
    }
    else
    {
        Logger::error("Calibration data not found in config file or is not an object");
    }






    // 在函数末尾添加验证代码
    Logger::debug("Verifying calibration parameters");
    
    bool parametersValid = true;
    const double EPSILON = 1e-10; // 浮点数比较的容差
    
    // 期望的VIC undistortion_k
    cv::Mat expected_K_vic = (cv::Mat_<double>(3, 3) << 
        7.5012863963332995e+02, 0.0, 9.7024677244355769e+02,
        0.0, 7.4985736273648990e+02, 5.3451263822206283e+02,
        0.0, 0.0, 1.0);
    
    // 期望的VIC undistortion_d
    cv::Mat expected_D_vic = (cv::Mat_<double>(1, 5) << 
        -2.5610207495984375e-02, 1.3959869417354318e-02, -5.6024477856761001e-04,
        1.6170662386737877e-03, -2.8092378160683904e-03);
    
    // 期望的IR undistortion_k
    cv::Mat expected_K_ir = (cv::Mat_<double>(3, 3) << 
        4.6657019094600258e+02, 0.0, 6.1436573148207879e+02,
        0.0, 4.6673738376504457e+02, 5.2876547888405139e+02,
        0.0, 0.0, 1.0);
    
    // 期望的IR undistortion_d
    cv::Mat expected_D_ir = (cv::Mat_<double>(1, 5) << 
        5.1113111471330193e-02, -2.4299638734852295e-02, 1.5429629398373440e-03,
        1.7167818085733531e-03, 1.6280272349683662e-03);
    
    // 验证VIC undistortion_k
    if (!K_vic.empty())
    {
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                if (std::abs(K_vic.at<double>(i, j) - expected_K_vic.at<double>(i, j)) > EPSILON)
                {
                    Logger::error("VIC undistortion_k parameter error at [" + 
                                std::to_string(i) + "][" + std::to_string(j) + "]: " +
                                // "expected " + std::to_string(expected_K_vic.at<double>(i, j)) + 
                                ", got " + std::to_string(K_vic.at<double>(i, j)));
                    parametersValid = false;
                }
            }
        }
    }
    else
    {
        Logger::error("VIC undistortion_k is empty");
        parametersValid = false;
    }
    
    // 验证VIC undistortion_d
    if (!D_vic.empty())
    {
        for (int i = 0; i < 5; ++i)
        {
            if (std::abs(D_vic.at<double>(0, i) - expected_D_vic.at<double>(0, i)) > EPSILON)
            {
                Logger::error("VIC undistortion_d parameter error at [" + std::to_string(i) + "]: " +
                            // "expected " + std::to_string(expected_D_vic.at<double>(0, i)) + 
                            ", got " + std::to_string(D_vic.at<double>(0, i)));
                parametersValid = false;
            }
        }
    }
    else
    {
        Logger::error("VIC undistortion_d is empty");
        parametersValid = false;
    }
    
    // 验证IR undistortion_k
    if (!K_ir.empty())
    {
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                if (std::abs(K_ir.at<double>(i, j) - expected_K_ir.at<double>(i, j)) > EPSILON)
                {
                    Logger::error("IR undistortion_k parameter error at [" + 
                                std::to_string(i) + "][" + std::to_string(j) + "]: " +
                                // "expected " + std::to_string(expected_K_ir.at<double>(i, j)) + 
                                ", got " + std::to_string(K_ir.at<double>(i, j)));
                    parametersValid = false;
                }
            }
        }
    }
    else
    {
        Logger::error("IR undistortion_k is empty");
        parametersValid = false;
    }
    
    // 验证IR undistortion_d
    if (!D_ir.empty())
    {
        for (int i = 0; i < 5; ++i)
        {
            if (std::abs(D_ir.at<double>(0, i) - expected_D_ir.at<double>(0, i)) > EPSILON)
            {
                Logger::error("IR undistortion_d parameter error at [" + std::to_string(i) + "]: " +
                            // "expected " + std::to_string(expected_D_ir.at<double>(0, i)) + 
                            ", got " + std::to_string(D_ir.at<double>(0, i)));
                parametersValid = false;
            }
        }
    }
    else
    {
        Logger::error("IR undistortion_d is empty");
        parametersValid = false;
    }
    
    // 最终验证结果
    if (!parametersValid)
    {
        Logger::error("Distortion parameter error");
    }
    else
    {
        Logger::info("All calibration parameters verified successfully");
    }


}

/**
 * 加载LiDAR参数
 * 包括内参、外参和线程启用设置
 */
void Config::loadLidarParameters()
{
    Logger::debug("Loading LiDAR parameters");
    if (config_.contains("lidar_settings") && config_["lidar_settings"].is_object())
    {
        auto &lidar = config_["lidar_settings"];

        // 获取LiDAR线程启用状态
        lidar_thread_enabled_ = lidar.value("enable_thread", true);
        Logger::info("LiDAR thread enabled: " + std::to_string(lidar_thread_enabled_));

        // 加载内参参数
        if (lidar.contains("intrinsic") && lidar["intrinsic"].is_object())
        {
            auto &intrinsic = lidar["intrinsic"];
            lidar_intrinsic_.fx = intrinsic.value("fx", 0.0); // 焦距x
            lidar_intrinsic_.fy = intrinsic.value("fy", 0.0); // 焦距y
            lidar_intrinsic_.cx = intrinsic.value("cx", 0.0); // 主点x
            lidar_intrinsic_.cy = intrinsic.value("cy", 0.0); // 主点y
            Logger::debug("LiDAR intrinsic parameters loaded");
        }
        else
        {
            Logger::warning("LiDAR intrinsic parameters not found or incomplete");
        }

        // 加载外参矩阵
        if (lidar.contains("extrinsic") && lidar["extrinsic"].is_array())
        {
            lidar_extrinsic_ = cv::Mat(4, 4, CV_64F);
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    lidar_extrinsic_.at<double>(i, j) = lidar["extrinsic"][i][j];
                }
            }
            Logger::debug("LiDAR extrinsic parameters loaded");
        }
        else
        {
            Logger::warning("LiDAR extrinsic parameters not found or incomplete");
        }
    }
    else
    {
        Logger::error("LiDAR parameters not found in config file or is not an object");
    }
}

/**
 * 获取字符串类型的配置项值
 * @param key 配置项键名
 * @param defaultValue 默认值
 * @return 配置项的值或默认值
 */
std::string Config::getString(const std::string &key, const std::string &defaultValue) const
{
    return config_.value(key, defaultValue);
}

/**
 * 获取整型配置项值
 * @param key 配置项键名
 * @param defaultValue 默认值
 * @return 配置项的值或默认值
 */
int Config::getInt(const std::string &key, int defaultValue) const
{
    return config_.value(key, defaultValue);
}

/**
 * 获取浮点型配置项值
 * @param key 配置项键名
 * @param defaultValue 默认值
 * @return 配置项的值或默认值
 */
double Config::getDouble(const std::string &key, double defaultValue) const
{
    return config_.value(key, defaultValue);
}

/**
 * 获取布尔型配置项值
 * @param key 配置项键名
 * @param defaultValue 默认值
 * @return 配置项的值或默认值
 */
bool Config::getBool(const std::string &key, bool defaultValue) const
{
    return config_.value(key, defaultValue);
}

/**
 * 获取PJ IP地址
 * @return IP地址字符串
 */
std::string Config::getPjIpAddress() const
{
    return pj_ip_address_;
}

/**
 * 获取接收端口
 * @return 端口号
 */
int Config::getReceivePort() const
{
    return receive_port_;
}

/**
 * 获取发送端口
 * @return 端口号
 */
int Config::getSendPort() const
{
    return send_port_;
}

/**
 * 获取调试选项的值
 * @param option 选项名称
 * @return 选项的布尔值，未找到时返回false
 */
bool Config::getDebugOption(const std::string &option) const
{
    if (debug_options_.contains(option))
    {
        bool value = debug_options_[option].get<bool>();
        Logger::debug("Debug option '" + option + "' retrieved: " + (value ? "true" : "false"));
        return value;
    }
    Logger::warning("Debug option '" + option + "' not found, returning false");
    return false;
}

/**
 * 获取可见光相机串口
 * @return 串口设备路径
 */
std::string Config::getVisibleCameraSerialPort() const
{
    return visible_camera_serial_port_;
}

/**
 * 获取红外相机串口
 * @return 串口设备路径
 */
std::string Config::getInfraredCameraSerialPort() const
{
    return infrared_camera_serial_port_;
}

/**
 * 获取可见光相机内参矩阵K
 * @return 内参矩阵
 */
cv::Mat Config::getVicUndistortionK() const
{
    return K_vic;
}

/**
 * 获取可见光相机畸变系数D
 * @return 畸变系数向量
 */
cv::Mat Config::getVicUndistortionD() const
{
    return D_vic;
}

/**
 * 获取红外相机内参矩阵K
 * @return 内参矩阵
 */
cv::Mat Config::getIrUndistortionK() const
{
    return K_ir;
}

/**
 * 获取红外相机畸变系数D
 * @return 畸变系数向量
 */
cv::Mat Config::getIrUndistortionD() const
{
    return D_ir;
}

/**
 * 获取可见光-红外相机配准单应性矩阵H
 * @return 配准矩阵
 */
cv::Mat Config::getRegistrationH() const
{
    return H;
}

/**
 * 获取指定相机的图像宽度
 * @param camera 相机标识符
 * @return 图像宽度
 */
int Config::getImageWidth(const std::string &camera) const
{
    if (config_.contains("image_dimensions") &&
        config_["image_dimensions"].contains(camera) &&
        config_["image_dimensions"][camera].contains("width"))
    {
        return config_["image_dimensions"][camera]["width"];
    }
    Logger::error("Image width not found for camera: " + camera);
    return 0; // 或抛出异常
}

/**
 * 获取指定相机的图像高度
 * @param camera 相机标识符
 * @return 图像高度
 */
int Config::getImageHeight(const std::string &camera) const
{
    if (config_.contains("image_dimensions") &&
        config_["image_dimensions"].contains(camera) &&
        config_["image_dimensions"][camera].contains("height"))
    {
        return config_["image_dimensions"][camera]["height"];
    }
    Logger::error("Image height not found for camera: " + camera);
    return 0; // 或抛出异常
}

/**
 * 获取红外映射方法
 * @return 映射方法字符串
 */
std::string Config::getIRMappingMethod() const
{
    return ir_mapping_method_;
}

/**
 * 获取初始比特率
 * @return 比特率值
 */
int Config::getInitialBpsRate() const
{
    if (config_.contains("video_settings") && config_["video_settings"].contains("initial_bps_rate"))
    {
        return config_["video_settings"]["initial_bps_rate"].get<int>();
    }
    Logger::warning("Initial bps rate not found in config, using default value of 500000");
    return 500000; // 默认值
}

/**
 * 获取红外视频设备路径
 * @return 设备路径
 */
std::string Config::getIRVideoDevice() const
{
    if (config_.contains("video_devices") && config_["video_devices"].contains("ir"))
    {
        return config_["video_devices"]["ir"].get<std::string>();
    }
    Logger::warning("IR video device not found in config, using default value of /dev/video0");
    return "/dev/video0"; // 默认值
}

/**
 * 获取可见光视频设备路径
 * @return 设备路径
 */
std::string Config::getVisibleVideoDevice() const
{
    if (config_.contains("video_devices") && config_["video_devices"].contains("visible"))
    {
        return config_["video_devices"]["visible"].get<std::string>();
    }
    Logger::warning("Visible video device not found in config, using default value of /dev/video8");
    return "/dev/video8"; // 默认值
}

/**
 * 获取网络接口名称
 * @return 接口名称
 */
std::string Config::getNetInterfaceName() const
{
    return net_interface_name_;
}

/**
 * 获取默认混合类型
 * @return 混合类型枚举值
 */
BlendType Config::getDefaultBlendType() const
{
    return default_blend_type_;
}

/**
 * 获取RKNN模型路径
 * @return 模型文件路径
 */
std::string Config::getRKNNModelPath() const
{
    return rknn_model_path_;
}

/**
 * 获取标签名称文本文件路径
 * @return 文件路径
 */
std::string Config::getLabelNameTxtPath() const
{
    return label_name_txt_path_;
}

/**
 * 获取边界框阈值
 * @return 阈值
 */
float Config::getBoxThresh() const
{
    return box_thresh_;
}

/**
 * 获取非极大值抑制阈值
 * @return 阈值
 */
float Config::getNmsThresh() const
{
    return nms_thresh_;
}

/**
 * 获取对象类别数量
 * @return 类别数量
 */
int Config::getObjClassNum() const
{
    return obj_class_num_;
}

/**
 * 获取LiDAR数据端口
 * @return 端口号
 */
int Config::getLidarPortDat() const
{
    return lidar_port_dat_;
}

/**
 * 获取LiDAR设备端口
 * @return 端口号
 */
int Config::getLidarPortDev() const
{
    return lidar_port_dev_;
}

/**
 * 获取LiDAR IP地址
 * @return IP地址字符串
 */
std::string Config::getLidarIP() const
{
    return lidar_ip_;
}

/**
 * 确定本地IP地址和UDP发送端口
 * 根据网络接口名称获取本地IP地址，并基于IP地址末尾设置UDP发送端口和方向偏移量
 */
void Config::determineLocalIPAndPort()
{
    ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    // 获取所有网络接口信息
    if (getifaddrs(&ifaddr) == -1)
    {
        Logger::error("getifaddrs failed: " + std::string(strerror(errno)));
        return;
    }

    // 遍历所有网络接口，寻找配置的接口名称
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
            strcmp(ifa->ifa_name, net_interface_name_.c_str()) == 0)
        {
            // 找到匹配的接口，获取其IP地址
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0)
            {
                local_ip_ = host;
                break;
            }
        }
    }

    // 释放网络接口信息结构
    freeifaddrs(ifaddr);

    if (local_ip_.empty())
    {
        Logger::error("Failed to get local IP address");
        return;
    }

    // 根据IP地址末尾三位数字设置UDP发送端口和方向偏移量
    std::string localIpTail = local_ip_.substr(local_ip_.length() - 3);
    static const std::unordered_map<std::string, std::pair<int, int>> ipConfigMap = {
        {"100", {8878, 0}},   // 端口: 8878, 偏移: 0度
        {"110", {8879, 90}},  // 端口: 8879, 偏移: 90度
        {"120", {8880, 180}}, // 端口: 8880, 偏移: 180度
        {"130", {8881, 270}}  // 端口: 8881, 偏移: 270度
    };

    // 查找对应的端口和偏移配置
    auto it = ipConfigMap.find(localIpTail);
    if (it != ipConfigMap.end())
    {
        udp_send_port_ = it->second.first;
        offset_ = it->second.second; // 设置角度偏移
    }
    else
    {
        Logger::warning("Unexpected IP address tail: " + localIpTail + ". Using default values.");
        udp_send_port_ = 8878; // 默认端口
        offset_ = 0;           // 默认偏移
    }

    Logger::info("Local IP: " + local_ip_ + ", UDP Send Port: " + std::to_string(udp_send_port_) + ", Offset: " + std::to_string(offset_));
}

// ===== 网络配置获取函数 =====
// 这组函数用于获取系统网络相关的配置参数，包括本地IP地址和UDP发送端口

// 获取配置中的本地IP地址
std::string Config::getLocalIP() const
{
    return local_ip_; // 返回存储的本地IP地址
}

// 获取配置的UDP发送端口号
int Config::getUDPSendPort() const
{
    return udp_send_port_; // 返回UDP发送端口号
}

// 获取激光雷达(LiDAR)线程是否启用的配置
bool Config::getLidarThreadEnabled() const
{
    return lidar_thread_enabled_; // 返回激光雷达线程的启用状态
}

// ===== 调试选项加载函数 =====
// 本函数负责从配置文件中加载调试相关的选项，并设置全局调试参数

// 在Config.cpp中实现
void Config::loadDebugOptions()
{
    // 检查配置中是否包含DEBUG_OPTIONS键，且值是JSON对象
    if (config_.contains("DEBUG_OPTIONS") && config_["DEBUG_OPTIONS"].is_object())
    {
        // 存储调试选项到类成员变量
        debug_options_ = config_["DEBUG_OPTIONS"];
        Logger::debug("Debug options loaded"); // 记录调试日志

        // 加载图像保存相关选项
        bool save_images = debug_options_.value("save_images", false); // 默认不保存图像
        int save_interval = debug_options_.value("save_interval", 5);  // 默认保存间隔5秒
        // 记录图像保存选项到日志
        Logger::info("Image saving options: Save Images=" + std::to_string(save_images) +
                     ", Save Interval=" + std::to_string(save_interval) + " seconds");

        // 加载本地边界框绘制选项
        draw_bounding_boxes_locally_ = debug_options_.value("draw_bounding_boxes_locally", false);
        Logger::info("Draw bounding boxes locally: " + std::to_string(draw_bounding_boxes_locally_));

        // 加载岸线本地绘制选项
        draw_shore_line_locally_ = debug_options_.value("draw_shore_line_locally", false);
        Logger::info("Draw shore line locally: " + std::to_string(draw_shore_line_locally_));

        // 加载严格检测模式选项
        strict_detection_mode_ = debug_options_.value("strict_detection_mode", true); // 默认启用严格检测
        Logger::info("Strict detection mode: " + std::to_string(strict_detection_mode_));
    }
    else
    {
        // 配置文件中没有找到DEBUG_OPTIONS或格式不正确，记录警告
        Logger::warning("DEBUG_OPTIONS not found in config file or is not an object");
    }
}

// ===== 调试选项获取函数集 =====
// 这组函数用于获取各种调试相关的配置选项

// 获取是否保存图像的配置
bool Config::getSaveImages() const
{
    return debug_options_.value("save_images", false); // 默认不保存图像
}

// 获取图像保存间隔(秒)
int Config::getSaveInterval() const
{
    return debug_options_.value("save_interval", 5); // 默认间隔5秒
}

// 获取是否在本地绘制边界框
bool Config::getDrawBoundingBoxesLocally() const
{
    return draw_bounding_boxes_locally_; // 返回边界框本地绘制状态
}

// 获取是否在本地绘制岸线
bool Config::getDrawShoreLineLocally() const
{
    return draw_shore_line_locally_; // 返回岸线本地绘制状态
}

// ===== 状态帧调试修改获取函数 =====
// 本函数从配置中解析状态帧的调试修改信息，用于网络通信调试

// 获取状态帧调试修改配置
std::map<int, uint8_t> Config::getStatusFrameDebugModifications() const
{
    std::map<int, uint8_t> modifications; // 创建存储修改信息的映射
    // 检查配置中是否存在状态帧修改的配置项
    if (config_.find("DEBUG_OPTIONS") != config_.end() &&
        config_["DEBUG_OPTIONS"].find("status_frame_modifications") != config_["DEBUG_OPTIONS"].end() &&
        config_["DEBUG_OPTIONS"]["status_frame_modifications"].is_object())
    {
        // 遍历所有状态帧修改项
        for (const auto &item : config_["DEBUG_OPTIONS"]["status_frame_modifications"].items())
        {
            std::string key = item.key();                             // 获取字节索引
            std::string binary_str = item.value().get<std::string>(); // 获取二进制字符串
            if (binary_str.length() == 8)                             // 确保二进制字符串长度为8位
            {
                std::bitset<8> bits(binary_str);                            // 将二进制字符串转换为位集
                uint8_t byte_value = static_cast<uint8_t>(bits.to_ulong()); // 转换为字节值
                modifications[std::stoi(key)] = byte_value;                 // 添加到映射
            }
            else
            {
                // 二进制字符串长度不正确，记录警告
                Logger::warning("Invalid binary string length for byte " + key + ": " + binary_str);
            }
        }
    }
    return modifications; // 返回状态帧修改映射
}

// 获取是否启用严格检测模式
bool Config::getStrictDetectionMode() const
{
    return strict_detection_mode_; // 返回严格检测模式状态
}

// ===== 图像和模型配置获取函数 =====
// 这组函数用于获取输出图像尺寸和模型类型的配置

// 获取输出图像宽度
int Config::getOutWidth() const
{
    return outputImgWidth_; // 返回输出图像宽度
}

// 获取输出图像高度
int Config::getOutHeight() const
{
    return outputImgHeight_; // 返回输出图像高度
}

// 判断当前模型是否为量化模型
bool Config::isQuantizedModel() const
{
    return is_quantized_model_; // 返回模型量化状态
}

// ===== 中心范围限制获取函数 =====
// 这组函数用于获取中心点Y坐标的范围限制，可能用于目标检测或跟踪

// 获取中心Y坐标的最小值
float Config::getCenterYMin() const
{
    return center_y_min_; // 返回中心Y坐标最小值
}

// 获取中心Y坐标的最大值
float Config::getCenterYMax() const
{
    return center_y_max_; // 返回中心Y坐标最大值
}

/**
 * 加载多边形过滤区域参数
 * 在Config构造函数中调用此函数，具体位置可以在loadDebugOptions()后面
 */
void Config::loadFilterPolygon()
{
    Logger::debug("Loading filter polygon parameters");
    
    // 默认的多边形区域为空
    filter_polygon_.clear();
    
    // 检查配置中是否包含过滤多边形参数
    if (config_.contains("filter_polygon") && config_["filter_polygon"].is_array())
    {
        // 清空默认值，准备加载配置文件中的值
        filter_polygon_.clear();
        
        // 遍历配置中的顶点数组
        for (const auto& point : config_["filter_polygon"])
        {
            if (point.is_array() && point.size() == 2)
            {
                int x = point[0].get<int>();
                int y = point[1].get<int>();
                filter_polygon_.push_back({x, y});
                Logger::debug("Loaded polygon point: (" + std::to_string(x) + ", " + std::to_string(y) + ")");
            }
            else
            {
                Logger::warning("Invalid point format in filter_polygon config, expected [x, y] array");
            }
        }
        
        Logger::info("Filter polygon loaded with " + std::to_string(filter_polygon_.size()) + " points");
    }
    else
    {
        Logger::info("No filter polygon defined by default. Objects will not be filtered by region.");
    }
}

/**
 * 获取多边形过滤区域的顶点
 * @return 顶点坐标对的向量
 */
const std::vector<std::pair<int, int>>& Config::getFilterPolygon() const
{
    return filter_polygon_;
}










/**
 * 加载UDP发送控制参数
 * 从配置文件的network_settings部分读取UDP数据包发送控制参数
 */
void Config::loadUDPSendParameters()
{
    Logger::debug("Loading UDP send parameters");
    
    // 检查网络设置是否存在
    if (config_.contains("network_settings") && config_["network_settings"].is_object())
    {
        auto &network = config_["network_settings"];
        
        // 检查是否存在UDP发送控制参数配置
        if (network.contains("udp_send_control") && network["udp_send_control"].is_object())
        {
            auto &udp_control = network["udp_send_control"];
            
            // 加载数据包间隔参数
            if (udp_control.contains("packet_interval") && udp_control["packet_interval"].is_number())
            {
                int interval = udp_control["packet_interval"].get<int>();
                if (interval > 0)
                {
                    udp_packet_interval_ = interval;
                    Logger::info("UDP packet interval loaded: " + std::to_string(udp_packet_interval_));
                }
                else
                {
                    Logger::warning("Invalid UDP packet interval value: " + std::to_string(interval) + 
                                   ", using default: " + std::to_string(udp_packet_interval_));
                }
            }
            else
            {
                Logger::info("UDP packet interval not specified, using default: " + std::to_string(udp_packet_interval_));
            }
            
            // 加载休眠时间参数
            if (udp_control.contains("sleep_duration_ms") && udp_control["sleep_duration_ms"].is_number())
            {
                int duration = udp_control["sleep_duration_ms"].get<int>();
                if (duration >= 0)  // 允许0值（不休眠）
                {
                    udp_sleep_duration_ms_ = duration;
                    Logger::info("UDP sleep duration loaded: " + std::to_string(udp_sleep_duration_ms_) + "ms");
                }
                else
                {
                    Logger::warning("Invalid UDP sleep duration value: " + std::to_string(duration) + 
                                   ", using default: " + std::to_string(udp_sleep_duration_ms_) + "ms");
                }
            }
            else
            {
                Logger::info("UDP sleep duration not specified, using default: " + std::to_string(udp_sleep_duration_ms_) + "ms");
            }
            
            // 记录加载完成的参数
            Logger::info(std::string("UDP send parameters loaded - Packet Interval: ") + std::to_string(udp_packet_interval_) + 
                        ", Sleep Duration: " + std::to_string(udp_sleep_duration_ms_) + "ms");
        }
        else
        {
            Logger::info(std::string("UDP send control parameters not found in config, using defaults - ") +
                        "Packet Interval: " + std::to_string(udp_packet_interval_) + 
                        ", Sleep Duration: " + std::to_string(udp_sleep_duration_ms_) + "ms");
        }
    }
    else
    {
        Logger::warning("network_settings not found in config file, using default UDP send parameters");
    }
}

/**
 * 获取UDP数据包发送间隔
 * @return 每发送多少个数据包后休眠一次
 */
int Config::getUDPPacketInterval() const
{
    Logger::debug("Getting UDP packet interval: " + std::to_string(udp_packet_interval_));
    return udp_packet_interval_;
}

/**
 * 获取UDP发送休眠时间
 * @return 休眠时间（毫秒）
 */
int Config::getUDPSleepDuration() const
{
    Logger::debug("Getting UDP sleep duration: " + std::to_string(udp_sleep_duration_ms_) + "ms");
    return udp_sleep_duration_ms_;
}

/**
 * 设置UDP数据包发送间隔
 * @param interval 数据包发送间隔，必须大于0
 */
void Config::setUDPPacketInterval(int interval)
{
    if (interval > 0)
    {
        udp_packet_interval_ = interval;
        Logger::info("UDP packet interval updated to: " + std::to_string(udp_packet_interval_));
    }
    else
    {
        Logger::warning("Invalid UDP packet interval: " + std::to_string(interval) + 
                       ", value must be greater than 0");
    }
}

/**
 * 设置UDP发送休眠时间
 * @param duration 休眠时间（毫秒），必须大于等于0
 */
void Config::setUDPSleepDuration(int duration)
{
    if (duration >= 0)
    {
        udp_sleep_duration_ms_ = duration;
        Logger::info("UDP sleep duration updated to: " + std::to_string(udp_sleep_duration_ms_) + "ms");
    }
    else
    {
        Logger::warning("Invalid UDP sleep duration: " + std::to_string(duration) + 
                       ", value must be greater than or equal to 0");
    }
}