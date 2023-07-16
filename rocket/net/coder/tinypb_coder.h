#ifndef ROCKET_NET_CODER_TINYPB_CODER_H
#define ROCKET_NET_CODER_TINYPB_CODER_H

#include "rocket/net/coder/abstract_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"

namespace rocket {

class TinyPBCoder : public AbstractCoder {
public:
    TinyPBCoder() {}
    ~TinyPBCoder() {}
    // 将message对象转换为字节流 写入到buffer
    void encode(std::vector<AbstractProtocol::s_ptr>& messages, TcpBuffer::s_ptr out_buffer);

    // 将buffer中的字节流 转换为message
    void decode(std::vector<AbstractProtocol::s_ptr>& out_messages, TcpBuffer::s_ptr buffer);

private:
    const char* encodeTinyPB(std::shared_ptr<TinyPBProtocol> message, int& len);
};


}



#endif