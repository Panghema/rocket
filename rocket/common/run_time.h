#ifndef ROCKET_COMMON_RUNTIME_H
#define ROCKET_COMMON_RUNTIME_H

#include <string>
// #include <rpc_interface.h>

namespace rocket
{

class RpcInterface;

class RunTime {
public:
    RpcInterface* getRpcInterface();

public:
    static RunTime* GetRunTime();
    
public:
    std::string m_msgid;
    std::string m_method_name;
    RpcInterface* m_rpc_interface {NULL};

};

} // namespace rocket




#endif