#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include <string>

// LOG_INFO("%s %d, arg1, arg2")
#define LOG_INFO(logMsgFormat, ...) \
    do { \
        Logger* logger = Logger::instance(); \
        logger->setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logMsgFormat, ##__VA_ARGS__); \
        logger->log(buf); \
    } while(0)

#define LOG_ERROR(logMsgFormat, ...) \
    do { \
        Logger* logger = Logger::instance(); \
        logger->setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logMsgFormat, ##__VA_ARGS__); \
        logger->log(buf); \
    } while(0)

#define LOG_FATAL(logMsgFormat, ...) \
    do { \
        Logger* logger = Logger::instance(); \
        logger->setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logMsgFormat, ##__VA_ARGS__); \
        logger->log(buf); \
    } while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(logMsgFormat, ...) \
    do { \
        Logger* logger = Logger::instance(); \
        logger->setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logMsgFormat, ##__VA_ARGS__); \
        logger->log(buf); \
    } while(0) 
#else
    #define LOG_DEBUG(logMsgFormat, ...)
#endif

// 定义日志级别
// INFO 正常日志输出
// ERROR 不影响运行的错误 
// FATAL 严重错误
// DEBUG 调试信息

enum LogLevel{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

// 输出一个日志类
class Logger : noncopyable{
    public:
        // 获取日志唯一的实例对象
        static Logger* instance();
        // 设置日志级别
        void setLogLevel(int Level);
        // 写日志
        void log(std::string msg);
        ~Logger(){
            if (log_ != nullptr){
                delete log_;
            }
        }
    private:
        static Logger* log_;
        int LogLevel_;
        Logger(){}
};