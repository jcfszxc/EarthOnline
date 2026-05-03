/**
 * @file GetLidarData_CX128S2.h
 * @brief CX128S2型号激光雷达数据获取类的头文件
 *
 * 该类继承自GetLidarData基类，专门用于处理CX128S2型号激光雷达的数据获取、
 * 坐标转换以及设备参数配置等功能。
 */

#pragma once // 防止头文件被多次包含

#include "GetLidarData.h" // 包含基类头文件

/**
 * @class GetLidarData_CX128S2
 * @brief CX128S2型号激光雷达数据获取与处理专用类
 *
 * 该类实现了激光雷达数据的获取、点云坐标计算、设备状态控制等功能
 */
class GetLidarData_CX128S2 : public GetLidarData
{
public:
	GetLidarData_CX128S2();	 // 构造函数
	~GetLidarData_CX128S2(); // 析构函数

	void LidarRun() override;									  // 重写基类的激光雷达运行方法
	m_PointXYZ XYZ_calculate(int, float, float, float Distance2); // 坐标转换公式  参数Distance2: <=0时，不计算第二回波数据

	bool setLidarRotateState(int StateValue, std::string &InfoString) override;	   // 设置激光雷达旋转/静止状态
	bool setLidarSoureSelection(int StateValue, std::string &InfoString) override; // 设置时间源选择
	bool setLidarWorkState(int StateValue, std::string &InfoString) override;	   // 设置激光雷达工作状态，是否为低功耗模式

	virtual bool getLidarParamState(LidarStateParam &mLidarStateParam, std::string &InfoString) override; // 获取激光雷达参数状态

private:
	float sinTheta_1[128]; // 第一组通道的正弦角度值数组
	float sinTheta_2[128]; // 第二组通道的正弦角度值数组
	float cosTheta_1[128]; // 第一组通道的余弦角度值数组
	float cosTheta_2[128]; // 第二组通道的余弦角度值数组

	void getOffsetAngle(std::vector<int> OffsetAngleValue); // 获取激光雷达偏移角度

	int count = 0;								// 同步帧判断计数器
	void handleSingleEcho(unsigned char *data); // 单回波数据处理
	void handleDoubleEcho(unsigned char *data); // 双回波数据处理
};