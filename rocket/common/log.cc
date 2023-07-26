#include <sys/time.h>
#include <sstream>
#include <stdio.h>
#include "rocket/common/log.h"
#include "rocket/common/util.h"
#include "rocket/common/config.h"
// #include "log.h"
#include "rocket/net/eventloop.h"
#include "rocket/common/runtime.h"
namespace rocket {

static Logger* g_logger = NULL;

Logger* Logger::GetGlobalLogger() {
    return g_logger;
}

// 1异步 0同步
Logger::Logger(LogLevel level, int type) : m_set_level(level), m_type(type){
    if (m_type == 0) {
        return;
    }
    m_asnyc_logger = std::make_shared<AsyncLogger>(
        Config::GetGlobalConfig()->m_log_file_path,
        Config::GetGlobalConfig()->m_log_file_name + "_rpc",
        Config::GetGlobalConfig()->m_log_max_file_size);
    
    m_asnyc_app_logger = std::make_shared<AsyncLogger>(
        Config::GetGlobalConfig()->m_log_file_path,
        Config::GetGlobalConfig()->m_log_file_name + "_app",
        Config::GetGlobalConfig()->m_log_max_file_size);
     
}

void Logger::init() {
    if (m_type == 0) {
        return;
    }

    m_timer_event = std::make_shared<TimerEvent>(
        Config::GetGlobalConfig()->m_log_sync_interval,
        true, std::bind(&Logger::syncLoop, this));

    EventLoop::getCurrentEventLoop()->addTimerEvent(m_timer_event);
}

void Logger::syncLoop() {
    // 同步m_buffer到async_logger 的buffer队尾
    std::vector<std::string> tmp_vec;
    ScopeMutex<Mutex> lock(m_mutex);
    tmp_vec.swap(m_buffer);
    lock.unlock();

    // 
    if (!tmp_vec.empty()) {
        m_asnyc_logger->pushLogBuffer(tmp_vec);
    }
    tmp_vec.clear();

    // applog
    std::vector<std::string> tmp_vec2;
    ScopeMutex<Mutex> app_lock(m_app_mutex);
    tmp_vec2.swap(m_app_buffer);
    app_lock.unlock();

    // 
    if (!tmp_vec2.empty()) {
        m_asnyc_app_logger->pushLogBuffer(tmp_vec2);
    }
    tmp_vec2.clear();

}

void Logger::InitGlobalLogger(int type)
{
    if (g_logger == NULL) {
        LogLevel global_log_level = StringToLogLevel(Config::GetGlobalConfig()->m_log_level);
        printf("Init log level [%s]\n", Config::GetGlobalConfig()->m_log_level.c_str());
        g_logger = new Logger(global_log_level, type);
    }

    g_logger->init();
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

    // 获取当前线程处理的请求的msgid
    std::string msgid = RunTime::GetRunTime()->m_msgid;
    std::string method_name = RunTime::GetRunTime()->m_method_name;
    if (!msgid.empty()) {
        ss << "[" << msgid << "]\t";
    }
        if (!method_name.empty()) {
        ss << "[" << method_name << "]\t";
    }

    return ss.str();

}

void Logger::pushLog(const std::string& msg) {
    if (m_type == 0) {
        printf((msg + "\n").c_str());
        return;
    }
    ScopeMutex<Mutex> lock(m_mutex);
    m_buffer.push_back(msg);
    lock.unlock(); // 其实可以不加，析构就自动解锁了 更正：必须不加，不然md就core了
}

void Logger::pushAppLog(const std::string& msg) {
    ScopeMutex<Mutex> lock(m_app_mutex);
    m_app_buffer.push_back(msg);
    lock.unlock(); // 其实可以不加，析构就自动解锁了 更正：必须不加，不然md就core了
}

void Logger::log() {
    // ScopeMutex<Mutex> lock(m_mutex);
    // std::queue<std::string> temp;
    // temp.swap(m_buffer);
    // lock.unlock();
    // while (!temp.empty()) {
    //     std::string msg = temp.front();
    //     temp.pop();

    //     printf("%s", msg.c_str());
    // }
}


AsyncLogger::AsyncLogger(const std::string& file_path, const std::string& file_name, int max_size) 
    : m_file_path(file_path), m_file_name(file_name), m_max_file_size(max_size) {
    
    sem_init(&m_semaphore, 0, 0);

    pthread_create(&m_thread, NULL, &AsyncLogger::Loop, this);

    // pthread_cond_init(&m_condition, NULL);

    sem_wait(&m_semaphore);
}

void* AsyncLogger::Loop(void* arg) {
    // 将buffer里面的数据都打印到文件中，然后线程睡眠直到有新的数据
    AsyncLogger* logger = reinterpret_cast<AsyncLogger*>(arg);

    pthread_cond_init(&logger->m_condition, NULL); // ???

    sem_post(&logger->m_semaphore);

    while(1) {
        ScopeMutex<Mutex> lock(logger->m_mutex);
        while (logger->m_buffer.empty()) {
            pthread_cond_wait(&(logger->m_condition), logger->m_mutex.getMutex());
        }
        std::vector<std::string> tmp;
        tmp.swap(logger->m_buffer.front());
        logger->m_buffer.pop();

        lock.unlock();

        timeval now;
        gettimeofday(&now, NULL);

        struct tm now_time;
        localtime_r(&(now.tv_sec), &now_time);

        const char* format = "%Y%m%d";
        char date[32];
        strftime(date, sizeof(date), format, &now_time);

        if (std::string(date) != logger->m_date) {
            logger->m_no = 0;
            logger->m_reopen_flag = true;
            logger->m_date = std::string(date);
        }

        if (logger->m_file_handler == NULL) {
            logger->m_reopen_flag = true;
        }

        std::stringstream ss;
        ss << logger->m_file_path << logger->m_file_name << "_"
            << std::string(date) << "_log.";
        std::string log_file_name = ss.str() + std::to_string(logger->m_no);

        if (logger->m_reopen_flag) {
            if (logger->m_file_handler) {
                fclose(logger->m_file_handler);
            }
            logger->m_file_handler = fopen(log_file_name.c_str(), "a");
            logger->m_reopen_flag = false;
        }

        if (ftell(logger->m_file_handler) > logger->m_max_file_size) {
            fclose(logger->m_file_handler);
          
            log_file_name = ss.str() + std::to_string(logger->m_no++);
            logger->m_file_handler = fopen(log_file_name.c_str(), "a");
            logger->m_reopen_flag = false;
        }

        for (auto& i: tmp) {
            if (!i.empty()) {
                fwrite(i.c_str(), 1, i.length(), logger->m_file_handler);
            }
        }
        fflush(logger->m_file_handler);

        if (logger->m_stop_flag) {
            return NULL;
        }
    }

    return NULL;
}

void AsyncLogger::stop() {
    m_stop_flag = true;
}

void AsyncLogger::flush() {
    if (m_file_handler) {
        fflush(m_file_handler);
    }
}

void AsyncLogger::pushLogBuffer(std::vector<std::string>& vec) {
    ScopeMutex<Mutex> lock(m_mutex);
    m_buffer.push(vec);

    pthread_cond_signal(&m_condition);
    
    lock.unlock();
}

}