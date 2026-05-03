/**
 * @文件: Logger.cpp
 * @描述: 一个简单的日志记录类的实现，支持多个日志级别和文件输出
 * 
 * 该日志类实现了一个基本的日志系统，可以记录不同级别的日志信息到文件中。
 * 支持的日志级别包括：DEBUG, INFO, WARNING, ERROR, FATAL
 * 日志记录包含时间戳和日志级别标签
 */

 #include "Logger.h"  // 包含Logger类的头文件
 #include <iostream>  // 用于标准输入输出
 #include <fstream>   // 用于文件操作
 #include <ctime>     // 用于时间相关功能
 #include <iomanip>   // 用于格式化输出
 
 // 静态成员变量的初始化
 std::ofstream Logger::logFile_;  // 日志文件流对象
 Logger::LogLevel Logger::currentLogLevel_ = Logger::LogLevel::INFO;  // 默认日志级别为INFO
 
 /**
  * 初始化日志系统
  * @param filename 日志文件名
  * @param level 日志级别
  */
 void Logger::init(const std::string& filename, LogLevel level) {
     logFile_.open(filename, std::ios::app);  // 以追加模式打开日志文件
     if (!logFile_.is_open()) {  // 检查文件是否成功打开
         std::cerr << "Unable to open log file: " << filename << std::endl;  // 输出错误信息
         return;
     }
     currentLogLevel_ = level;  // 设置当前日志级别
 }
 
 /**
  * 记录日志的核心函数
  * @param level 日志级别
  * @param message 日志消息
  */
 void Logger::log(LogLevel level, const std::string& message) {
     if (level < currentLogLevel_) return;  // 如果日志级别低于当前设置的级别，则不记录
     
     std::time_t now = std::time(nullptr);  // 获取当前时间
     std::tm* localTime = std::localtime(&now);  // 转换为本地时间
     
     logFile_ << "[" << std::put_time(localTime, "%Y-%m-%d %H:%M:%S") << "] ";  // 写入时间戳
     
     // 根据日志级别写入对应的标签
     switch (level) {
         case LogLevel::DEBUG:   logFile_ << "[DEBUG] "; break;
         case LogLevel::INFO:    logFile_ << "[INFO] "; break;
         case LogLevel::WARNING: logFile_ << "[WARNING] "; break;
         case LogLevel::ERROR:   logFile_ << "[ERROR] "; break;
         case LogLevel::FATAL:   logFile_ << "[FATAL] "; break;
     }
     
     logFile_ << message << std::endl;  // 写入日志消息并换行
     logFile_.flush();  // 刷新文件流，确保日志立即写入文件
 }
 
 /**
  * 记录DEBUG级别的日志
  * @param message 日志消息
  */
 void Logger::debug(const std::string& message) {
     log(LogLevel::DEBUG, message);  // 调用log函数记录DEBUG级别的日志
 }
 
 /**
  * 记录INFO级别的日志
  * @param message 日志消息
  */
 void Logger::info(const std::string& message) {
     log(LogLevel::INFO, message);  // 调用log函数记录INFO级别的日志
 }
 
 /**
  * 记录WARNING级别的日志
  * @param message 日志消息
  */
 void Logger::warning(const std::string& message) {
     log(LogLevel::WARNING, message);  // 调用log函数记录WARNING级别的日志
 }
 
 /**
  * 记录ERROR级别的日志
  * @param message 日志消息
  */
 void Logger::error(const std::string& message) {
     log(LogLevel::ERROR, message);  // 调用log函数记录ERROR级别的日志
 }
 
 /**
  * 记录FATAL级别的日志
  * @param message 日志消息
  */
 void Logger::fatal(const std::string& message) {
     log(LogLevel::FATAL, message);  // 调用log函数记录FATAL级别的日志
 }