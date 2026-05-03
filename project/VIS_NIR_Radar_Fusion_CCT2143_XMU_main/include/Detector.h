/**
 * @file Detector.h
 * @brief 目标检测器头文件
 *
 * 本文件定义了一个基于RKNN的目标检测器类，主要用于实现YOLOv5等目标检测模型的推理功能。
 * 该检测器能够处理图像输入，输出检测到的目标位置、类别和置信度信息。
 * 支持量化模型(int8)和浮点模型(float)两种处理方式。
 */

#ifndef DETECTOR_H
#define DETECTOR_H

#include "Config.h"   // 包含配置类定义
#include "rknn_api.h" // 包含RKNN API接口定义

#define OBJ_NAME_MAX_SIZE 16 // 定义目标名称最大长度为16字节
#define OBJ_NUMB_MAX_SIZE 64 // 定义单帧图像中最大目标数量为64个

/**
 * @struct BOX_RECT
 * @brief 定义边界框结构体，表示目标检测的矩形区域
 */
typedef struct _BOX_RECT
{
    int left;   // 左边界坐标
    int right;  // 右边界坐标
    int top;    // 上边界坐标
    int bottom; // 下边界坐标
} BOX_RECT;

/**
 * @struct detect_result_t
 * @brief 单个检测结果的数据结构
 */
typedef struct __detect_result_t
{
    char name[OBJ_NAME_MAX_SIZE];                    // 目标类别名称
    BOX_RECT box;                                    // 目标边界框
    float prop;                                      // 目标置信度
    std::chrono::steady_clock::time_point timestamp; // 检测时间戳
} detect_result_t;

/**
 * @struct detect_result_group_t
 * @brief 检测结果组，存储一帧图像中所有检测到的目标
 */
typedef struct _detect_result_group_t
{
    int id;                                     // 结果组ID
    int count;                                  // 检测到的目标数量
    detect_result_t results[OBJ_NUMB_MAX_SIZE]; // 所有检测结果数组
} detect_result_group_t;

/**
 * @class Detector
 * @brief 目标检测器类，封装了基于RKNN的目标检测功能
 */
class Detector
{
public:
    /**
     * @brief 构造函数
     * @param config 配置对象，包含模型路径等信息
     */
    Detector(const Config &config);

    /**
     * @brief 析构函数
     */
    ~Detector();

    /**
     * @brief 初始化RKNN模型
     */
    void initializeRKNN();

    /**
     * @brief 执行目标检测
     * @param frame_data 输入图像数据
     * @return 检测结果组
     */
    detect_result_group_t detect(unsigned char *frame_data);

    /**
     * @brief 获取模型输入通道数
     * @return 通道数
     */
    int getChannel() const { return channel_; }

    /**
     * @brief 获取模型输入高度
     * @return 高度像素数
     */
    int getModelHeight() const { return model_height_; }

    /**
     * @brief 获取模型输入宽度
     * @return 宽度像素数
     */
    int getModelWidth() const { return model_width_; }

private:
    Config config_;                              // 配置信息
    rknn_context ctx_;                           // RKNN上下文
    rknn_input_output_num io_num_;               // 输入输出数量
    std::vector<rknn_tensor_attr> input_attrs_;  // 输入张量属性
    std::vector<rknn_tensor_attr> output_attrs_; // 输出张量属性
    int channel_;                                // 输入通道数
    int model_height_;                           // 模型输入高度
    int model_width_;                            // 模型输入宽度
    int obj_class_num_;                          // 目标类别数量
    int prop_box_size_;                          // 每个预测框的属性数量
    std::vector<char *> labels_;                 // 标签名称列表

    // YOLOv5模型使用的三组锚点框尺寸配置
    const int anchor0[6] = {10, 13, 16, 30, 33, 23};      // 小目标锚点
    const int anchor1[6] = {30, 61, 62, 45, 59, 119};     // 中目标锚点
    const int anchor2[6] = {116, 90, 156, 198, 373, 326}; // 大目标锚点

    /**
     * @brief 加载标签名称文件
     * @param locationFilename 标签文件路径
     * @return 成功返回0，失败返回错误码
     */
    int loadLabelName(const char *locationFilename);

    /**
     * @brief 处理量化模型的单个特征图输出
     * @param input 特征图数据
     * @param anchor 当前特征图使用的锚点
     * @param grid_h 特征图高度
     * @param grid_w 特征图宽度
     * @param height 原始图像高度
     * @param width 原始图像宽度
     * @param stride 步长
     * @param boxes 输出的边界框坐标
     * @param objProbs 输出的目标置信度
     * @param classId 输出的类别ID
     * @param threshold 置信度阈值
     * @param zp 量化零点
     * @param scale 量化比例
     * @return 处理的边界框数量
     */
    int process(int8_t *input, int *anchor, int grid_h, int grid_w, int height, int width, int stride,
                std::vector<float> &boxes, std::vector<float> &objProbs, std::vector<int> &classId, float threshold,
                int32_t zp, float scale);

    /**
     * @brief 量化模型的后处理函数
     * @param input0 第一个输出特征图
     * @param input1 第二个输出特征图
     * @param input2 第三个输出特征图
     * @param model_in_h 模型输入高度
     * @param model_in_w 模型输入宽度
     * @param conf_threshold 置信度阈值
     * @param nms_threshold 非极大值抑制阈值
     * @param pads 图像填充信息
     * @param scale_w 宽度缩放比例
     * @param scale_h 高度缩放比例
     * @param qnt_zps 量化零点数组
     * @param qnt_scales 量化比例数组
     * @param group 检测结果输出组
     * @return 成功返回0
     */
    int post_process(int8_t *input0, int8_t *input1, int8_t *input2, int model_in_h, int model_in_w,
                     float conf_threshold, float nms_threshold, BOX_RECT pads, float scale_w, float scale_h,
                     std::vector<int32_t> &qnt_zps, std::vector<float> &qnt_scales,
                     detect_result_group_t *group);

    /**
     * @brief 处理浮点模型的单个特征图输出
     * @param input 特征图数据
     * @param anchor 当前特征图使用的锚点
     * @param grid_h 特征图高度
     * @param grid_w 特征图宽度
     * @param height 原始图像高度
     * @param width 原始图像宽度
     * @param stride 步长
     * @param boxes 输出的边界框坐标
     * @param objProbs 输出的目标置信度
     * @param classId 输出的类别ID
     * @param threshold 置信度阈值
     * @return 处理的边界框数量
     */
    int process_float(float *input, int *anchor, int grid_h, int grid_w,
                      int height, int width, int stride,
                      std::vector<float> &boxes, std::vector<float> &objProbs,
                      std::vector<int> &classId, float threshold);

    /**
     * @brief 浮点模型的后处理函数
     * @param input0 第一个输出特征图
     * @param input1 第二个输出特征图
     * @param input2 第三个输出特征图
     * @param model_in_h 模型输入高度
     * @param model_in_w 模型输入宽度
     * @param conf_threshold 置信度阈值
     * @param nms_threshold 非极大值抑制阈值
     * @param pads 图像填充信息
     * @param scale_w 宽度缩放比例
     * @param scale_h 高度缩放比例
     * @param group 检测结果输出组
     * @return 成功返回0
     */
    int post_process_float(float *input0, float *input1, float *input2,
                           int model_in_h, int model_in_w,
                           float conf_threshold, float nms_threshold,
                           BOX_RECT pads, float scale_w, float scale_h,
                           detect_result_group_t *group);
};

#endif // DETECTOR_H