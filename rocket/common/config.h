#ifndef ROCKET_COMMON_CONFIG_H
#define ROCKET_COMMON_CONFIG_H

#include <string>

namespace rocket {

class Config{
public:
    Config(const char* xmlfile);
    Config();

public:
    static Config* GetGlobalConfig();
    static void SetGlobalConfig(const char* xmlfile);

public:
    std::string m_log_level;
    std::string m_log_file_name;
    std::string m_log_file_path;
    int m_log_max_file_size {0};
    int m_log_sync_interval {0}; // 日志异步记录间隔 ms

    int m_port {0};
    int m_io_threads {0};

};



}


#endif