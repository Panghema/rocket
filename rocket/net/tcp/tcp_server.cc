#include "rocket/net/tcp/tcp_server.h"
#include "rocket/net/eventloop.h"
#include "rocket/common/log.h"

#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/common/config.h"

namespace rocket
{

    TcpServer::TcpServer(NetAddr::s_ptr local_addr) : m_local_addr(local_addr)
    {

        init();
        INFOLOG("rocket TcpServer listen successfully on [%s]", m_local_addr->toString().c_str());
    }

    TcpServer::~TcpServer()
    {
        if (m_main_event_loop)
        {
            delete m_main_event_loop;
            m_main_event_loop = NULL;
        }
    }


    // TODO:15:32处任务，把准备结束的connection设置成CLosed，定时任务定期地遍历m_client里的connection，
    // 如果是closed状态就从m_client里删掉，反正是智能指针删掉了就自己西沟了
    void TcpServer::onAccept()
    {
        auto re = m_acceptor->accept();
        int client_fd = re.first;
        NetAddr::s_ptr peer_addr = re.second;
        // FdEvent client_fd_event(client_fd);
        m_client_counts++;

        DEBUGLOG("TcpServer succ get client, fd=%d", client_fd);

        // TODO:把clientfd添加到io线程中
        // 任意一个线程
        // m_io_thread_group->getIOThread()->getEventLoop()->addEpollEvent();
        IOThread *io_thread = m_io_thread_group->getIOThread();
        TcpConnection::s_ptr connection = std::make_shared<TcpConnection>(io_thread->getEventLoop(), client_fd, 128, peer_addr, m_local_addr, TcpConnectionByServer);
        connection->setState(Connected);
        m_client.insert(connection);
        INFOLOG("TcpServer succ get client, fd=%d", client_fd);
    }

    void TcpServer::init()
    {
        m_acceptor = std::make_shared<TcpAcceptor>(m_local_addr);

        m_main_event_loop = EventLoop::getCurrentEventLoop();

        m_io_thread_group = new IOThreadGroup(rocket::Config::GetGlobalConfig()->m_io_threads);

        m_listen_fd_event = new FdEvent(m_acceptor->getListenFd());

        m_listen_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpServer::onAccept, this), nullptr);
        
        m_main_event_loop->addEpollEvent(m_listen_fd_event);
    }

    void TcpServer::start()
    {
        DEBUGLOG("hello");
        m_io_thread_group->start();

        m_main_event_loop->loop();
    }

}