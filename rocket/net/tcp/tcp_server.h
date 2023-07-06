#ifndef ROCKET_NET_TCP_SERVER_H
#define ROCKET_NET_TCP_SERVER_H

#include <set>
#include "rocket/net/tcp/tcp_acceptor.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/io_thread_group.h"

namespace rocket
{
// tcp server是一个全局的单例对象
// 全局的eventloop 就是main reactor
class TcpServer {

public:
    TcpServer(NetAddr::s_ptr local_addr);

    ~TcpServer();

    void start();

private:
    void init();

    // 当有新客户端链接后需要执行
    void onAccept();

private:
    TcpAcceptor::s_ptr m_acceptor;

    NetAddr::s_ptr m_local_addr; // 本地监听的地址

    EventLoop* m_main_event_loop {NULL}; // main reactor

    IOThreadGroup* m_io_thread_group {NULL}; // subReactor

    FdEvent* m_listen_fd_event;

    int m_client_counts {0};

    std::set<TcpConnection::s_ptr> m_client; // 持久化用的
};

} // namespace rocket



#endif