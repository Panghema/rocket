#ifndef ROCKET_NET_ABSTRACT_CODER_H
#define ROCKET_NET_ABSTRACT_CODER_H

#include <vector>
#include "rocket/net/tcp/tcp_buffer.h"
#include "rocket/net/abstract_protocol.h"

namespace rocket {

class AbstractCoder {

public:

    // 将message对象转换为字节流 写入到buffer
    virtual void encoder(std::vector<AbstractProtocol::s_ptr>& messages, TcpBuffer::s_ptr out_buffer) = 0;

    // 将buffer中的字节流 转换为message
    virtual void decoder(std::vector<AbstractProtocol::s_ptr>& messages, TcpBuffer::s_ptr in_buffer) = 0;

    virtual ~AbstractCoder() {}
};


}

#endif