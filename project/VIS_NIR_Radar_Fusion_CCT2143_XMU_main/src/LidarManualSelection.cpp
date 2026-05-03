/**
 * 激光雷达手动选择模块
 *
 * 这个模块实现了对激光雷达数据的手动选择功能。它允许用户在深度图像上选择一个区域，
 * 并计算该区域内的平均距离。主要功能包括：
 * 1. 处理控制帧以获取用户选择
 * 2. 计算选定区域的矩形范围
 * 3. 计算所选区域内的平均距离
 * 4. 在图像上绘制选定区域和距离信息
 */

#include "LidarManualSelection.h"
#include <numeric>
#include <algorithm>

// // 构造函数，初始化选择状态为非激活
// LidarManualSelection::LidarManualSelection() : is_active(false) {}

// /**
//  * 处理控制帧数据，获取用户的选择操作
//  * @param control_frame 控制帧数据指针
//  */
// void LidarManualSelection::processControlFrame(const uint8_t *control_frame)
// {
//     // 从控制帧的第19字节获取操作模式
//     ctrl.mode = static_cast<LidarManualSelectionMode>(control_frame[19]);
//     if (ctrl.mode != LidarManualSelectionMode::NO_ACTION)
//     {
//         // 从控制帧的第20-23字节获取选择的坐标位置
//         ctrl.x = *reinterpret_cast<const uint16_t *>(control_frame + 20);
//         ctrl.y = *reinterpret_cast<const uint16_t *>(control_frame + 22);
//         // 从控制帧的第24字节获取选择区域的大小
//         ctrl.size = static_cast<LidarManualSelectionSize>(control_frame[24]);
//         // 只有当模式为激活时才设置活动状态为true
//         is_active = (ctrl.mode == LidarManualSelectionMode::ACTIVATE);
//     }
// }

// /**
//  * 计算感兴趣区域(ROI)的矩形范围
//  * @return 包含选定区域的OpenCV矩形
//  */
// cv::Rect LidarManualSelection::calculateROIRect() const
// {
//     // 根据选定的尺寸枚举确定实际像素大小
//     int size;
//     switch (ctrl.size)
//     {
//     case LidarManualSelectionSize::S16:
//         size = 16;
//         break;
//     case LidarManualSelectionSize::S32:
//         size = 32;
//         break;
//     case LidarManualSelectionSize::S64:
//         size = 64;
//         break;
//     default:
//         size = 16; // 默认使用最小尺寸
//     }
//     // 返回以选择点为中心的矩形区域
//     return cv::Rect(ctrl.x - size / 2, ctrl.y - size / 2, size, size);
// }

// /**
//  * 计算选定区域内的平均距离
//  * @param depth_map 深度图像
//  */
// void LidarManualSelection::calculateAverageDistance(const cv::Mat &depth_map)
// {
//     // 从深度图像中提取ROI区域
//     cv::Mat roi_depth = depth_map(roi.rect);
//     // 存储有效深度值的向量
//     std::vector<float> depths;
//     // 遍历ROI区域中的每个像素
//     roi_depth.forEach<float>([&](float &pixel, const int *position)
//                              {
//          // 只收集大于0的有效深度值
//          if (pixel > 0) depths.push_back(pixel); });
//     // 计算平均距离
//     if (!depths.empty())
//     {
//         roi.ave_distance = std::accumulate(depths.begin(), depths.end(), 0.0) / depths.size();
//     }
//     else
//     {
//         roi.ave_distance = 0;
//     }
//     // 注意：这里应该有俯仰角和方位角的计算
// }

// /**
//  * 更新用于显示的格式化文本
//  */
// void LidarManualSelection::updateFormattedText()
// {
//     // 格式化距离文本，保留两位小数
//     formatted_txt.label = cv::format("%.2fm", roi.ave_distance);
//     // 计算文本在图像上的尺寸
//     cv::Size text_size = cv::getTextSize(formatted_txt.label, formatted_txt.style.face,
//                                          formatted_txt.style.scale, formatted_txt.style.thickness,
//                                          &formatted_txt.baseline);
//     formatted_txt.size = text_size;
// }

// /**
//  * 根据深度图像更新ROI区域信息
//  * @param depth_map 深度图像
//  */
// void LidarManualSelection::updateROI(const cv::Mat &depth_map)
// {
//     // 如果选择不处于激活状态，直接返回
//     if (!is_active)
//         return;

//     // 计算ROI矩形区域
//     roi.rect = calculateROIRect();
//     // 计算该区域的平均距离
//     calculateAverageDistance(depth_map);
//     // 更新要显示的文本
//     updateFormattedText();
// }

// /**
//  * 在图像上绘制ROI区域和距离信息
//  * @param image 要绘制的图像
//  */
// void LidarManualSelection::drawROI(cv::Mat &image)
// {
//     // 如果选择不处于激活状态，直接返回
//     if (!is_active)
//         return;

//     // 在图像上绘制矩形框，绿色，线宽2
//     cv::rectangle(image, roi.rect, cv::Scalar(0, 255, 0), 2);
//     // 计算文本放置位置（矩形上方）
//     cv::Point text_org(roi.rect.x, roi.rect.y - formatted_txt.baseline - 5);
//     // 在图像上绘制距离文本
//     cv::putText(image, formatted_txt.label, text_org, formatted_txt.style.face, formatted_txt.style.scale,
//                 formatted_txt.style.color, formatted_txt.style.thickness);
// }