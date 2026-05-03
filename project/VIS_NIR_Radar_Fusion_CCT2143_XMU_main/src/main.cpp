/**
 * 代码摘要：
 * 这是一个C++应用程序的主入口点。程序初始化日志系统，加载配置文件，
 * 创建并运行工作流，然后进入无限循环以保持程序运行。程序也包含了
 * 异常处理机制来捕获和记录任何运行时错误。
 */

 #include <iostream>      // 包含标准输入输出流库
 #include "Logger.h"      // 包含日志记录系统
 #include "Config.h"      // 包含配置管理类
 #include "WorkFlow.h"    // 包含工作流管理类
 
 int main(int argc, char *argv[])  // 程序入口点，带命令行参数
 {
     try  // 尝试执行主程序逻辑
     {
         // 初始化日志系统
         Logger::init("application.log");  // 设置日志文件名为"application.log"
         LOG_INFO("\n\n\n");  // 记录三个空行到日志中，用于分隔日志条目
         LOG_INFO("Application starting...");  // 记录应用程序启动信息
 
         // 加载配置
         auto config = std::make_unique<Config>("config.json");  // 创建配置对象并加载"config.json"文件
         LOG_INFO("Configuration loaded");  // 记录配置加载完成信息
 
         // 创建并运行工作流
         auto workflow = std::make_unique<WorkFlow>(*config);  // 创建工作流对象，传入配置
         workflow->initialize();  // 初始化工作流
         workflow->run();  // 运行工作流
 
         while (true)  // 无限循环，保持程序运行
         {
             std::this_thread::sleep_for(std::chrono::seconds(1));  // 线程休眠1秒，减少CPU使用率
         }
 
         // 注意：以下代码永远不会执行，因为上面的循环是无限的
         LOG_INFO("Application shutting down normally");  // 记录应用程序正常关闭信息
         return 0;  // 返回成功退出代码
     }
     catch (const std::exception &e)  // 捕获标准异常
     {
         LOG_ERROR(LOG_FORMAT("Unhandled exception: " << e.what()));  // 记录未处理的异常信息
         return 1;  // 返回错误退出代码
     }
     catch (...)  // 捕获所有其他类型的异常
     {
         LOG_ERROR("Unknown exception occurred");  // 记录未知异常信息
         return 1;  // 返回错误退出代码
     }
 }