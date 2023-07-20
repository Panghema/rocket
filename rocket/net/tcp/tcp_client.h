#ifndef ROCKET_NET_TCP_TCP_CLIENT_H
#define ROCKET_NET_TCP_TCP_CLIENT_H

#include <memory>
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/coder/abstract_protocol.h"

namespace rocket {

class TcpClient {
public:
    typedef std::shared_ptr<TcpClient> s_ptr;
    TcpClient(NetAddr::s_ptr peer_addr);

    ~TcpClient();

    // 异步进行connect，回调用以感知connect结果
    // 调用connect成功，done会被执行
    void connect(std::function<void()> done);

    // 异步发送message（对象），通用协议
    // message如果发送成功，就会调用done函数，函数的入参就是message对象
    void writeMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done);

    // 异步读取message（对象），通用协议
    // message如果读取成功，就会调用done函数，函数的入参就是message对象
    void readMessage(const std::string& msg_id, std::function<void(AbstractProtocol::s_ptr)> done);

    void stop();
private:
    NetAddr::s_ptr m_peer_addr;
    EventLoop* m_event_loop {NULL};

    int m_fd {-1};
    FdEvent* m_fd_event {NULL};
    TcpConnection::s_ptr m_connection;


};




}




#endif