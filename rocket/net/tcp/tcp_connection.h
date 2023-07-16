#ifndef ROCKET_NET_TCP_TCP_CONNECTION_H
#define ROCKET_NET_TCP_TCP_CONNECTION_H

#include <memory>
#include <queue>
#include <vector>
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_buffer.h"
#include "rocket/net/io_thread.h"
#include "rocket/net/coder/string_coder.h"
#include "rocket/net/coder/tinypb_coder.h""

namespace rocket {

enum TcpState {
    NotConnected = 1,
    Connected = 2,
    HalfClosing = 3,
    Closed = 4
};

enum TcpConnectionType {
    TcpConnectionByServer = 1, // 作为服务端，表示一个客户端连接
    TcpConnectionByClient = 2, // 作为客户端使用，代表对端服务端连接
};

class TcpConnection {

public:
    typedef std::shared_ptr<TcpConnection> s_ptr;
    TcpConnection(EventLoop* event_loop, int fd, int buffer_size, NetAddr::s_ptr peer_addr, TcpConnectionType type=TcpConnectionByServer);

    ~TcpConnection();

    void onRead();
    
    void excute();

    void onWrite();

    void setState(const TcpState state);

    TcpState getState();


    // 结束释放
    void clear();

    // 服务器主动关闭连接 处理恶意连接之类的
    void shutdown();

    void setConnectionType(TcpConnectionType type);

    // 启动监听可写事件
    void listenWrite();

    void listenRead();

    void pushSendMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done);
    
    void pushReadMessage(const std::string& req_id, std::function<void(AbstractProtocol::s_ptr)> done);

private:
    EventLoop* m_event_loop {NULL};
    // IOThread* m_io_thread {NULL}; // 代表持有该连接的io线程方便当前指向的操作线程
    NetAddr::s_ptr m_peer_addr;
    TcpState m_state;
    int m_fd {0};

    NetAddr::s_ptr m_local_addr;

    TcpBuffer::s_ptr m_in_buffer; // 接受缓存区
    TcpBuffer::s_ptr m_out_buffer; // 发送缓存区

    FdEvent* m_fd_event {NULL};

    TcpConnectionType m_connection_type {TcpConnectionByServer};

    // std::pair<AbstractProtocol::s_ptr, std::function<void(AbstractProtocol::s_ptr)>> 
    std::vector<std::pair<AbstractProtocol::s_ptr, std::function<void(AbstractProtocol::s_ptr)>>> m_write_dones;

    std::map<std::string, std::function<void(AbstractProtocol::s_ptr)>> m_read_dones;
    
    AbstractCoder* m_coder {NULL};
};


}



#endif