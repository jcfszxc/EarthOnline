/**
 * WorkFlowUtils.cpp - 工作流工具实用模块
 *
 * 本文件实现了设备工作流程中的多种控制和管理功能。
 * 主要功能包括：
 * 1. 系统控制 - 自检、重启和关机管理、稳定模式切换
 * 2. OTA升级 - 处理设备升级显示、启动升级流程和版本管理
 * 3. 日志管理 - 系统日志的保存和处理功能
 * 4. 视觉处理 - 焦点区域设置、检测功能的启用/禁用
 * 5. 图像融合 - 支持多种图像混合模式（可见光、红外、激光及其组合）
 * 6. 安全管理 - 在执行重要操作（如重启、关机）前进行必要的同步和安全检查
 * 7. 命令响应 - 接收外部命令并执行相应的系统操作
 *
 * 该模块主要用于嵌入式设备的工作流程控制和系统管理。
 */

#include "WorkFlowUtils.h" // 包含工作流工具头文件
#include "Logger.h"        // 包含日志记录器头文件
#include <cstdlib>         // 包含C标准库，提供system()等函数
#include <array>           // 包含数组容器
#include <algorithm>       // 包含算法库，提供find_if()等函数

namespace WorkFlowUtils
{ // 定义工作流工具命名空间

    // 启动自检函数
    void startSelfTest(uint8_t selftest)
    { // 接收一个8位无符号整数作为自检标志
        if (selftest)
        {                                      // 如果自检标志非零
            LOG_INFO("Starting self-test..."); // 记录开始自检的信息
            // Implement self-test logic  // 此处应实现自检逻辑
        }
        else
        {                                   // 如果自检标志为零
            LOG_INFO("Self-test disabled"); // 记录自检已禁用的信息
        }
    }

    // 注释掉的updateBlendType函数，用于根据融合格式更新混合类型
    // void updateBlendType(uint8_t fusion_format, BlendType& m_blendType)
    // {
    //     LOG_INFO("Updating blend type with fusion format: 0x" + std::to_string(fusion_format));  // 记录更新混合类型的信息

    //     static const std::array<std::pair<uint8_t, BlendType>, 7> blendTypeMap = {{  // 定义融合格式与混合类型的映射关系
    //         {0x00, BlendType::V},    // 可见光模式
    //         {0x01, BlendType::I},    // 红外模式
    //         {0x02, BlendType::VL},   // 可见光+激光模式
    //         {0x03, BlendType::IL},   // 红外+激光模式
    //         {0x04, BlendType::VI},   // 可见光+红外模式
    //         {0x05, BlendType::VIL},  // 可见光+红外+激光模式
    //         {0x07, BlendType::L}     // 激光模式
    //     }};

    //     auto it = std::find_if(blendTypeMap.begin(), blendTypeMap.end(),  // 查找匹配的混合类型
    //                            [fusion_format](const auto& pair) { return pair.first == fusion_format; });

    //     if (it != blendTypeMap.end()) {  // 如果找到匹配的混合类型
    //         m_blendType = it->second;  // 设置混合类型
    //         LOG_INFO("Blend type updated successfully to: " + std::to_string(static_cast<int>(m_blendType)));  // 记录混合类型更新成功
    //     } else {  // 如果没有找到匹配的混合类型
    //         m_blendType = BlendType::V;  // 默认为可见光模式
    //         LOG_INFO("Invalid fusion format. Defaulting to blend type V");  // 记录使用默认混合类型
    //     }

    //     LOG_INFO("Final blend type: " + std::to_string(static_cast<int>(m_blendType)));  // 记录最终的混合类型
    // }

    // 切换稳定模式函数
    void switchStabilizationMode(uint8_t stabilization_mode)
    {                                                                                       // 接收一个8位无符号整数作为稳定模式
        LOG_INFO("Switching stabilization mode to: " + std::to_string(stabilization_mode)); // 记录切换稳定模式的信息
        // Implement stabilization mode switch logic  // 此处应实现稳定模式切换逻辑
    }

    // 处理重启请求函数
    void handleReboot(uint8_t reboot)
    {                                                                   // 接收一个8位无符号整数作为重启标志
        LOG_INFO("Handling reboot request: " + std::to_string(reboot)); // 记录处理重启请求的信息
        switch (reboot)
        {                                                      // 根据重启标志选择操作
        case 0x00:                                             // 如果重启标志为0x00
            LOG_INFO("Normal operation. No reboot required."); // 记录正常操作，无需重启
            break;
        case 0x01:                               // 如果重启标志为0x01
            LOG_INFO("Rebooting the system..."); // 记录正在重启系统
            sync();                              // 同步文件系统缓冲区
            if (std::system("reboot") != 0)
            {                                                                 // 执行重启命令
                std::cerr << "Failed to execute reboot command" << std::endl; // 如果重启命令执行失败，输出错误信息
            }
            break;
        case 0x02:                                   // 如果重启标志为0x02
            LOG_INFO("Shutting down the system..."); // 记录正在关闭系统
            sync();                                  // 同步文件系统缓冲区
            if (std::system("poweroff") != 0)
            {                                                                   // 执行关机命令
                std::cerr << "Failed to execute poweroff command" << std::endl; // 如果关机命令执行失败，输出错误信息
            }
            break;
        default:                                                                     // 如果重启标志为其他值
            LOG_ERROR("Unknown reboot command received: " + std::to_string(reboot)); // 记录收到未知重启命令的错误
            break;
        }
    }

    // 处理日志保存函数
    void handleLogSave(uint8_t log_save)
    {                                                               // 接收一个8位无符号整数作为日志保存标志
        LOG_INFO("Handling log save: " + std::to_string(log_save)); // 记录处理日志保存的信息
        // Implement log save logic  // 此处应实现日志保存逻辑
    }

    // 处理OTA升级显示函数
    void handleOtaUpgradeDisplay(uint8_t ota_upgrade_display)
    {                                                                                     // 接收一个8位无符号整数作为OTA升级显示标志
        LOG_INFO("Handling OTA upgrade display: " + std::to_string(ota_upgrade_display)); // 记录处理OTA升级显示的信息
        // Implement OTA upgrade display logic  // 此处应实现OTA升级显示逻辑
    }

    // 处理OTA升级开始函数
    void handleOtaUpgradeStart(uint8_t ota_upgrade_start)
    {                                                                                 // 接收一个8位无符号整数作为OTA升级开始标志
        LOG_INFO("Handling OTA upgrade start: " + std::to_string(ota_upgrade_start)); // 记录处理OTA升级开始的信息
        // Implement OTA upgrade start logic  // 此处应实现OTA升级开始逻辑
    }

    // 处理OTA版本函数
    void handleOtaVersion(uint8_t ota_version)
    {                                                                     // 接收一个8位无符号整数作为OTA版本
        LOG_INFO("Handling OTA version: " + std::to_string(ota_version)); // 记录处理OTA版本的信息
        // Implement OTA version handling logic  // 此处应实现OTA版本处理逻辑
    }

    // 处理焦点区域低阈值函数
    void handleFocusAreaLow(uint16_t focus_area_low)
    {                                                                           // 接收一个16位无符号整数作为焦点区域低阈值
        LOG_INFO("Handling focus area low: " + std::to_string(focus_area_low)); // 记录处理焦点区域低阈值的信息
        // Implement focus area low handling logic  // 此处应实现焦点区域低阈值处理逻辑
    }

    // 处理焦点区域高阈值函数
    void handleFocusAreaHigh(uint16_t focus_area_high)
    {                                                                             // 接收一个16位无符号整数作为焦点区域高阈值
        LOG_INFO("Handling focus area high: " + std::to_string(focus_area_high)); // 记录处理焦点区域高阈值的信息
        // Implement focus area high handling logic  // 此处应实现焦点区域高阈值处理逻辑
    }

    // 更新启用检测函数
    void updateEnableDetection(uint8_t char_overlay)
    {                                                                                             // 接收一个8位无符号整数作为字符叠加标志
        LOG_INFO("Updating enable detection with char_overlay: " + std::to_string(char_overlay)); // 记录更新启用检测的信息

        if (char_overlay == 0x01)
        {                                   // 如果字符叠加标志为0x01
            LOG_INFO("Enabling detection"); // 记录启用检测
            // Add specific implementation for enabling detection  // 此处应添加启用检测的具体实现
            // For example: DetectionSystem::enable();  // 例如：DetectionSystem::enable();
        }
        else
        {                                    // 如果字符叠加标志不为0x01
            LOG_INFO("Disabling detection"); // 记录禁用检测
            // Add specific implementation for disabling detection  // 此处应添加禁用检测的具体实现
            // For example: DetectionSystem::disable();  // 例如：DetectionSystem::disable();
        }
    }

} // namespace WorkFlowUtils  // 结束WorkFlowUtils命名空间