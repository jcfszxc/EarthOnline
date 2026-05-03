/**
 * @file LIDAR_MANUAL_SELECTION_H
 * @brief LiDAR手动选择区域头文件
 * 
 * 该文件定义了LiDAR手动选择功能的类和枚举，用于在深度图上选择和分析感兴趣区域（ROI）。
 * 通过这个类，用户可以在LiDAR深度图上选择特定区域并获取该区域的平均距离、俯仰角和方位角信息。
 */

 #ifndef LIDAR_MANUAL_SELECTION_H
 #define LIDAR_MANUAL_SELECTION_H
 
 // #include <opencv2/opencv.hpp>  // OpenCV库头文件（当前被注释）
 #include <cstdint>               // 提供固定大小整数类型定义
 
 /**
  * @enum LidarManualSelectionMode
  * @brief LiDAR手动选择模式枚举
  * 
  * 定义手动选择区域的操作模式
  */
//  enum class LidarManualSelectionMode : uint8_t {
//      NO_ACTION = 0x0,    // 无动作模式
//      ACTIVATE = 0x1,     // 激活选择模式
//      DEACTIVATE = 0x2,   // 停用选择模式
//  };
 
//  /**
//   * @enum LidarManualSelectionSize
//   * @brief LiDAR手动选择区域大小枚举
//   * 
//   * 定义选择区域的尺寸选项
//   */
//  enum class LidarManualSelectionSize : uint8_t {
//      S16 = 0x0,   // 16 x 16 像素大小
//      S32 = 0x1,   // 32 x 32 像素大小
//      S64 = 0x2,   // 64 x 64 像素大小
//  };
 
//  /**
//   * @class LidarManualSelection
//   * @brief LiDAR手动选择区域类
//   * 
//   * 提供LiDAR深度图中手动选择和分析区域的功能
//   */
//  class LidarManualSelection {
//  public:
//      /**
//       * @brief 构造函数
//       * 初始化LiDAR手动选择对象
//       */
//      LidarManualSelection();
     
//      /**
//       * @brief 处理控制帧数据
//       * @param control_frame 控制帧数据指针
//       */
//      void processControlFrame(const uint8_t* control_frame);
     
//      /**
//       * @brief 根据深度图更新感兴趣区域(ROI)信息
//       * @param depth_map 深度图
//       */
//      void updateROI(const cv::Mat& depth_map);
     
//      /**
//       * @brief 在图像上绘制感兴趣区域
//       * @param image 要绘制的图像
//       */
//      void drawROI(cv::Mat& image);
     
//      /**
//       * @brief 检查是否激活
//       * @return 返回激活状态
//       */
//      bool isActive() const { return is_active; }
 
//  private:
//      /**
//       * @struct Control
//       * @brief 控制结构体
//       * 
//       * 存储手动选择的控制参数
//       */
//      struct Control {
//          LidarManualSelectionMode mode;  // 选择模式
//          uint16_t x;                     // X坐标
//          uint16_t y;                     // Y坐标
//          LidarManualSelectionSize size;  // 选择区域大小
//      };
 
//      /**
//       * @struct ROI
//       * @brief 感兴趣区域结构体
//       * 
//       * 存储感兴趣区域的位置和计算的参数
//       */
//      struct ROI {
//          cv::Rect rect;       // 矩形区域
//          double ave_distance; // 平均距离
//          double pitch;        // 俯仰角
//          double azimuth;      // 方位角
//      };
 
//      /**
//       * @struct FormattedText
//       * @brief 格式化文本结构体
//       * 
//       * 存储用于显示ROI信息的文本格式和内容
//       */
//      struct FormattedText {
//          struct {
//              double scale = 0.9;                      // 文本缩放比例
//              int face = cv::FONT_HERSHEY_SIMPLEX;     // 字体类型
//              int thickness = 2;                       // 文本线宽
//              cv::Scalar color = cv::Scalar(0, 0, 255); // 文本颜色（红色）
//          } style;
//          std::string label;   // 标签文本
//          cv::Size size;       // 文本大小
//          int baseline = 0;    // 基线位置
//      };
 
//      Control ctrl;            // 控制参数
//      bool is_active;          // 激活状态
//      ROI roi;                 // 感兴趣区域
//      FormattedText formatted_txt; // 格式化文本
 
//      /**
//       * @brief 计算ROI矩形区域
//       * @return 返回计算的矩形区域
//       */
//      cv::Rect calculateROIRect() const;
     
//      /**
//       * @brief 计算区域内的平均距离
//       * @param depth_map 深度图
//       */
//      void calculateAverageDistance(const cv::Mat& depth_map);
     
//      /**
//       * @brief 更新格式化文本
//       * 根据ROI信息更新要显示的文本
//       */
//      void updateFormattedText();
//  };
 
 #endif // LIDAR_MANUAL_SELECTION_H