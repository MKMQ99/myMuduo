#include "Logger.h"
#include <iostream>

// 获取日志唯一的实例对象
Logger* Logger::instance(){
    if (log_ == nullptr){
        log_ = new Logger();
    }
    return log_;
}

// 设置日志级别
void Logger::setLogLevel(int Level){
    LogLevel_ = Level;
}

// 写日志 [级别信息] time : msg
void Logger::log(std::string msg){
    switch (LogLevel_){
        case INFO :
            std::cout << "[INFO]";
            break;
        case ERROR :
            std::cout << "[ERROR]";
            break;
        case FATAL :
            std::cout << "[FATAL]";
            break;
        case DEBUG :
            std::cout << "[DEBUG]";
            break;
        default:
            break;
    }

    // 打印时间和msg
    std::cout << "print time" << " : " << msg << std::endl;
}