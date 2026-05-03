/**
 * @文件: Logger.h
 * @描述: 日志系统的头文件，定义了Logger类的接口和相关宏
 *
 * 该头文件定义了Logger类的公共接口，包括初始化、不同级别的日志记录方法，
 * 以及一些便于使用的宏定义。Logger类采用静态方法设计，便于全局使用。
 */

 #ifndef LOGGER_H  // 防止头文件重复包含的宏定义开始
 #define LOGGER_H
 
 #include <string>    // 包含string类的定义
 #include <sstream>   // 包含字符串流的定义
 
 /**
  * Logger类: 提供日志记录功能
  * 
  * 该类使用静态方法实现，无需实例化即可使用，适合作为全局日志工具。
  * 支持多个日志级别，可以配置最低记录级别，低于该级别的日志将被忽略。
  */
 class Logger {
 public:
     // 日志级别枚举，从低到高依次为：DEBUG, INFO, WARNING, ERROR, FATAL
     enum class LogLevel { DEBUG, INFO, WARNING, ERROR, FATAL };
     
     /**
      * 初始化日志系统
      * @param filename 日志文件名
      * @param level 日志记录的最低级别，默认为INFO
      */
     static void init(const std::string& filename, LogLevel level = LogLevel::INFO);
     
     /**
      * 记录DEBUG级别的日志
      * @param message 日志消息
      */
     static void debug(const std::string& message);
     
     /**
      * 记录INFO级别的日志
      * @param message 日志消息
      */
     static void info(const std::string& message);
     
     /**
      * 记录WARNING级别的日志
      * @param message 日志消息
      */
     static void warning(const std::string& message);
     
     /**
      * 记录ERROR级别的日志
      * @param message 日志消息
      */
     static void error(const std::string& message);
     
     /**
      * 记录FATAL级别的日志
      * @param message 日志消息
      */
     static void fatal(const std::string& message);
 
 private:
     static std::ofstream logFile_;          // 日志文件流对象，静态成员
     static LogLevel currentLogLevel_;       // 当前设置的日志级别，静态成员
     
     /**
      * 记录日志的内部实现方法
      * @param level 日志级别
      * @param message 日志消息
      */
     static void log(LogLevel level, const std::string& message);
 };
 
 // 定义日志记录宏，简化日志记录调用
 #define LOG_DEBUG(message) Logger::debug(message)      // DEBUG级别日志记录宏
 #define LOG_INFO(message) Logger::info(message)        // INFO级别日志记录宏
 #define LOG_WARNING(message) Logger::warning(message)  // WARNING级别日志记录宏
 #define LOG_ERROR(message) Logger::error(message)      // ERROR级别日志记录宏
 #define LOG_FATAL(message) Logger::fatal(message)      // FATAL级别日志记录宏
 
 // 辅助宏，用于格式化字符串
 // 该宏接受任何可以输出到流的对象，并将其转换为字符串
 #define LOG_FORMAT(stream) (static_cast<std::ostringstream&>(std::ostringstream() << stream).str())
 
 #endif // LOGGER_H  // 防止头文件重复包含的宏定义结束