#ifndef ROCKET_NET_TCP_TCP_CONNECTION_H
#define ROCKET_NET_TCP_TCP_CONNECTION_H

#include <memory>
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_buffer.h"
#include "rocket/net/io_thread.h"

namespace rocket {

enum TcpState {
    NotConnected = 1,
    Connected = 2,
    HalfClosing = 3,
    Closed = 4
};

class TcpConnection {

public:
    typedef std::shared_ptr<TcpConnection> s_ptr;
    TcpConnection(IOThread* io_thread, int fd, int buffer_size, NetAddr::s_ptr peer_addr);

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

private:
    IOThread* m_io_thread {NULL}; // 代表持有该连接的io线程方便当前指向的操作线程
    NetAddr::s_ptr m_peer_addr;
    TcpState m_state;
    int m_fd {0};

    NetAddr::s_ptr m_local_addr;

    TcpBuffer::s_ptr m_in_buffer; // 接受缓存区
    TcpBuffer::s_ptr m_out_buffer; // 发送缓存区

    FdEvent* m_fd_event {NULL};



};


}



#endif