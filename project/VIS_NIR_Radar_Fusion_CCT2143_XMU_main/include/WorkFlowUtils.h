/**
 * @file WORKFLOW_UTILS_H
 * @brief 工作流实用工具头文件
 * 
 * 本文件定义了一系列用于系统工作流控制的实用函数，
 * 包括自检、稳定模式切换、重启控制、日志管理、OTA升级
 * 以及焦点区域和检测功能的处理函数。
 */

 #ifndef WORKFLOW_UTILS_H    // 防止头文件重复包含的宏定义开始
 #define WORKFLOW_UTILS_H    // 定义头文件宏
 
 #include <cstdint>          // 包含标准整数类型定义
 #include <unistd.h>         // 包含POSIX操作系统API
 #include <iostream>         // 包含标准输入输出流
 
 namespace WorkFlowUtils {   // 工作流工具命名空间开始
 
 /**
  * 启动系统自检
  * @param selftest 自检模式参数
  */
 void startSelfTest(uint8_t selftest);
 
 /**
  * 切换稳定模式
  * @param stabilization_mode 稳定模式参数
  */
 void switchStabilizationMode(uint8_t stabilization_mode);
 
 /**
  * 处理系统重启
  * @param reboot 重启指令参数
  */
 void handleReboot(uint8_t reboot);
 
 /**
  * 处理日志保存
  * @param log_save 日志保存指令参数
  */
 void handleLogSave(uint8_t log_save);
 
 /**
  * 处理OTA升级显示
  * @param ota_upgrade_display OTA升级显示参数
  */
 void handleOtaUpgradeDisplay(uint8_t ota_upgrade_display);
 
 /**
  * 处理OTA升级启动
  * @param ota_upgrade_start OTA升级启动参数
  */
 void handleOtaUpgradeStart(uint8_t ota_upgrade_start);
 
 /**
  * 处理OTA版本
  * @param ota_version OTA版本参数
  */
 void handleOtaVersion(uint8_t ota_version);
 
 /**
  * 处理焦点区域低值
  * @param focus_area_low 焦点区域低值参数
  */
 void handleFocusAreaLow(uint16_t focus_area_low);
 
 /**
  * 处理焦点区域高值
  * @param focus_area_high 焦点区域高值参数
  */
 void handleFocusAreaHigh(uint16_t focus_area_high);
 
 /**
  * 更新启用检测状态
  * @param char_overlay 字符覆盖参数
  */
 void updateEnableDetection(uint8_t char_overlay);
 
 }  // namespace WorkFlowUtils    // 工作流工具命名空间结束
 
 #endif // WORKFLOW_UTILS_H       // 防止头文件重复包含的宏定义结束
 
 // 已注释掉的函数:
 // void updateBlendType(uint8_t fusion_format, BlendType& m_blendType);
 // void updateEncodeRate(uint8_t encode_rate);