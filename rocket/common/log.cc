#include <sys/time.h>
#include <sstream>
#include <stdio.h>
#include "rocket/common/log.h"
#include "rocket/common/util.h"
#include "rocket/common/config.h"
#include "log.h"

namespace rocket {

static Logger* g_logger = NULL;

Logger* Logger::GetGlobalLogger() {
    return g_logger;
}

void Logger::InitGlobalLogger()
{
    if (g_logger == NULL) {
        LogLevel global_log_level = StringToLogLevel(Config::GetGlobalConfig()->m_log_level);
        printf("Init log level [%s]\n", Config::GetGlobalConfig()->m_log_level.c_str());
        g_logger = new Logger(global_log_level);
    }
}

//枚举类LogLevel与字符串名称的互相转换
std::string LogLevelToString(LogLevel level) {
    switch (level)
    {
    case Debug:
        return "DEBUG";
        break;
    case Info:
        return "INFO";
        break;
    case Error:
        return "ERROR";
        break;
    default:
        return "UNKNOWN";
        break;
    }
}


LogLevel StringToLogLevel(const std::string& log_level) {
    if (log_level == "DEBUG") {
        return Debug;
    } else if (log_level == "INFO") {
        return Info;
    }else if (log_level == "ERROR") {
        return Error;
    }else {
        return Unknown;
    }
}


std::string LogEvent::toString() {
    struct timeval now_time;

    gettimeofday(&now_time, nullptr); // 获取的是时间戳到当前的秒和微秒

    struct tm now_time_t; // 详见 man localtime_r
    localtime_r(&(now_time.tv_sec), &now_time_t); // 通过秒 转成calendar time的时间信息

    char buf[128];
    strftime(&buf[0], 128, "%y-%m-%d %H:%M:%S", &now_time_t);
    std::string time_str(buf);
    int ms = now_time.tv_usec / 1000;
    time_str = time_str + "." + std::to_string(ms);

    m_pid = getPid();
    m_thread_id = getThreadId();

    std::stringstream ss;
    ss 
    << "[" << LogLevelToString(m_level) << "]\t"
    << "[" << time_str << "]\t"
    << "[" << m_pid << ":" << m_thread_id << "]\t";

    return ss.str();

}

void Logger::pushLog(const std::string& msg) {
    ScopeMutex<Mutex> lock(m_mutex);
    m_buffer.push(msg);
    lock.unlock(); // 其实可以不加，析构就自动解锁了 更正：必须不加，不然md就core了
}

void Logger::log() {
    ScopeMutex<Mutex> lock(m_mutex);
    std::queue<std::string> temp;
    temp.swap(m_buffer);
    lock.unlock();
    while (!temp.empty()) {
        std::string msg = temp.front();
        temp.pop();

        printf("%s", msg.c_str());
    }
}

}