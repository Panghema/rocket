#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include "rocket/common/log.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/fd_event_group.h"

namespace rocket {


TcpClient::TcpClient(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr) {
    m_event_loop = EventLoop::getCurrentEventLoop();
    m_fd = socket(peer_addr->getFamily(), SOCK_STREAM, 0);
    if (m_fd < 0) {
        ERRORLOG("TcpClient::TcpClient() error, failed to create fd");
        return;
    }

    m_fd_event = FdEventGroup::getFdEventGroup()->getFdEvent(m_fd);
    m_fd_event->setNonBlock(); 

    m_connection = std::make_shared<TcpConnection>(m_event_loop, m_fd, 128, peer_addr, TcpConnectionByClient);
    m_connection->setConnectionType(TcpConnectionByClient);

}

TcpClient::~TcpClient() {
    DEBUGLOG("TcpClient::~TcpClient()");
    if (m_fd > 0) {
        close(m_fd);
    }
}

void TcpClient::connect(std::function<void()> done) {
    int rt = ::connect(m_fd, m_peer_addr->getSockAddr(), m_peer_addr->getSockLen());
    if (rt == 0) {
        DEBUGLOG("connect success");
        if (done) {
            done();
        }
    } else if (rt == -1) {
        if (errno == EINPROGRESS) {
            // epoll 监听可写事件，然后判断错误码
            m_fd_event->listen(FdEvent::OUT_EVENT, [this, done]() {
                int error = 0;
                socklen_t err_len = sizeof(error);
                getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &err_len);

                bool is_connect_succ = false; 
                if (error == 0) {
                    DEBUGLOG("connect [%s] success", m_peer_addr->toString().c_str());
                    is_connect_succ = true;
                    m_connection->setState(Connected);

                } else {
                    ERRORLOG("connect error, errno=%d, error=%s", errno, strerror(errno));
                }

                // 需要去掉可写事件的监听
                // 听到了就别监听了！
                m_fd_event->cancel(FdEvent::OUT_EVENT);
                m_event_loop->addEpollEvent(m_fd_event);


                // 只有连接成功，才会执行回调函数
                if (is_connect_succ && done) {
                    done();
                }
            });

        // 思考为啥这样加入是对的， 第一次运行忘了这两行！
        m_event_loop->addEpollEvent(m_fd_event);
        if (!m_event_loop->isLooping()) {
            m_event_loop->loop();
        }
        
        } else {
            ERRORLOG("connect error, errno=%d, error=%s", errno, strerror(errno));
        }
    }
}

void TcpClient::writeMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done) {
    // 把message对象写入到Connection的buffer，done也写入
    // 启动connection可写
    m_connection->pushSendMessage(message, done);
    m_connection->listenWrite();
    
}


void TcpClient::readMessage(const std::string& req_id, std::function<void(AbstractProtocol::s_ptr)> done) {
    // 1.监听可读事件
    // 2.从buffer里decode解码，得到message对象，判断是否req_id相等，相等则读成功，执行回调
    m_connection->pushReadMessage(req_id, done);
    m_connection->listenRead();
}


}