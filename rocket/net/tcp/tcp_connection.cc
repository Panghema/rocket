#include <unistd.h>
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/common/log.h"
#include "rocket/net/coder/tinypb_coder.h"

namespace rocket {


TcpConnection::TcpConnection(EventLoop* event_loop, int fd, int buffer_size, NetAddr::s_ptr peer_addr, NetAddr::s_ptr local_addr, TcpConnectionType type/*=TcpConnectionByServer*/) 
    : m_event_loop(event_loop), m_peer_addr(peer_addr), m_local_addr(local_addr), m_state(NotConnected), m_fd(fd), m_connection_type(type) {
        m_in_buffer = std::make_shared<TcpBuffer>(buffer_size);
        m_out_buffer = std::make_shared<TcpBuffer>(buffer_size);

        // 获取FdEvent
        m_fd_event = FdEventGroup::getFdEventGroup()->getFdEvent(fd); 
        m_fd_event->setNonBlock();

        m_coder = new TinyPBCoder();
        if (m_connection_type == TcpConnectionByServer) {
            listenRead();
            m_dispatcher = std::make_shared<RpcDispatcher>();
        }
}

TcpConnection::~TcpConnection() {
    DEBUGLOG("~TcpConnection");
    if (m_coder) {
        delete m_coder;
        m_coder = nullptr;
    }
}

void TcpConnection::onRead() {
    // 从socket缓冲区，调用系统的read函数读取字节到in_buffer里

    if (m_state != Connected) {
        INFOLOG("onRead error, client has already disconnected, addr[%s], clientfd[%d]", m_peer_addr->toString().c_str(), m_fd);
        return;
    }

    bool is_read_all = false;
    bool is_close = false;

    while(!is_read_all) {
        if (m_in_buffer->writeAble() == 0) {
            m_in_buffer->resizeBuffer(2 * m_in_buffer->m_buffer.size());
        }
        int read_count = m_in_buffer->writeAble();
        int write_index = m_in_buffer->writeIndex();

        int rt = ::read(m_fd_event->getFd(), &(m_in_buffer->m_buffer[write_index]), read_count);
        DEBUGLOG("success read %d bytes from addr[%s], clientfd[%d]", rt, m_peer_addr->toString().c_str(), m_fd);
        if (rt > 0) {
            m_in_buffer->moveWriteIndex(rt);

            if (rt == read_count) {
                continue;
            } else if (rt < read_count){
                is_read_all = true;
                break;                
            }

        } else if (rt == 0){    
            is_close = true;
            break;
        } else if(rt == -1 && errno == EAGAIN) {
            is_read_all = true;
            break;
        }

    }

    if (is_close) {
        // 处理关闭连接 todo
        INFOLOG("peer closed, peer addr [%s], client [%d]", m_peer_addr->toString().c_str(), m_fd); 
        clear();
        return; // 如果这里不return 后面又走到excute，又添加epoll event又监听了
    }

    if (!is_read_all) {
        ERRORLOG("not read all data");
    }


    // todo:简单的echo，后面补充rpc协议解析
    excute();
}

void TcpConnection::excute() {
    if (m_connection_type == TcpConnectionByServer) {
        // 将rpc请求执行业务逻辑，获取rpc相应，在把rpc响应发送回去
        std::vector<AbstractProtocol::s_ptr> result;
        std::vector<AbstractProtocol::s_ptr> reply_messages;
        m_coder->decode(result, m_in_buffer);
        for (size_t i=0; i<result.size(); ++i) {
            // 服务端该这么做
            // 针对每个请求，调用rpc方法获取响应message。
            // 将响应message放入到发送缓冲区，监听可写事件回包
            INFOLOG("success get request [%s] from client[%s]", result[i]->m_req_id.c_str(), m_peer_addr->toString().c_str());
            
            std::shared_ptr<TinyPBProtocol> message = std::make_shared<TinyPBProtocol>();
            // message->m_pb_data = "hello. this is rocket rpc test data";
            // message->m_req_id = result[i]->m_req_id;
            m_dispatcher->dispatch(result[i], message, this);
            reply_messages.emplace_back(message);
            // m_out_buffer->writeToBuffer(msg.c_str(), msg.length());

        }

        m_coder->encode(reply_messages, m_out_buffer);

        listenWrite();
    } else {
        // 从buffer里decode解码，得到message对象，判断是否req_id相等，相等则读成功，执行回调
        std::vector<AbstractProtocol::s_ptr> result;
        m_coder->decode(result, m_in_buffer);
        for (size_t i=0; i<result.size(); ++i) {
            std::string req_id = result[i]->m_req_id;
            auto it = m_read_dones.find(req_id);
            if (it != m_read_dones.end()) {
                it->second(result[i]); // 用shared_from_this，必须要确保构造的时候就是智能指针
            }
        }
    }
 
}

void TcpConnection::TcpConnection::onWrite() {
    // 将当前outbuffer里的数据发送给client
    if (m_state != Connected) {
        INFOLOG("onWrite error, client has already disconnected, addr[%s], clientfd[%d]", m_peer_addr->toString().c_str(), m_fd);
        return;
    }

    if (m_connection_type == TcpConnectionByClient) {
        // 1将message encode 得到字节流
        // 2将数据写入到buffer里， 然后全部发送
        std::vector<AbstractProtocol::s_ptr> messages;
        for (size_t i=0; i<m_write_dones.size(); ++i) {
            messages.push_back(m_write_dones[i].first);
        }
        m_coder->encode(messages, m_out_buffer);
    }

    bool is_write_all = false;
    while (true) {
        if (m_out_buffer->readAble() == 0) {
            DEBUGLOG("no data need to be sent to client [%s]", m_peer_addr->toString().c_str());
            is_write_all = true;
            break;
        }
        int write_size = m_out_buffer->readAble();
        int read_index = m_out_buffer->readIndex();

        int rt = write(m_fd, &(m_out_buffer->m_buffer[read_index]), write_size);
        if (rt >= write_size) {
            DEBUGLOG("no data need to be sent to client [%s]", m_peer_addr->toString().c_str());
            is_write_all = true;
            break;
        } else if (rt == -1 && errno == EAGAIN) {
            // 发送缓冲区已满，不能再发送了
            // 直接等下次fd可写的时候再发送
            ERRORLOG("write data error, errno=EAGAIN and rt=-1");    
            break;            
        }
    }
    if (is_write_all) {
        m_fd_event->cancel(FdEvent::OUT_EVENT);
        m_event_loop->addEpollEvent(m_fd_event); // 这里是add，更新后的event add进去就是修改，没有的add进去就是添加
    }

    // 回调
    if (m_connection_type == TcpConnectionByClient) {
        for (size_t i=0; i<m_write_dones.size(); ++i) {
            m_write_dones[i].second(m_write_dones[i].first);
        }
    }
    m_write_dones.clear();
}

void TcpConnection::setState(const TcpState state) {
    m_state = state;
}

TcpState TcpConnection::getState() {
    return m_state;
}

void TcpConnection::clear() {
    // 处理一些关闭连接后的清理动作
    if (m_state == Closed) {
        return;
    }
    m_fd_event->cancel(FdEvent::IN_EVENT);
    m_fd_event->cancel(FdEvent::OUT_EVENT);
    m_event_loop->deleteEpollEvent(m_fd_event);
    m_state = Closed;
}

void TcpConnection::shutdown() {
    if (m_state == Closed || m_state == NotConnected) {
        return;
    }
 
    // 处于半关闭状态
    m_state = HalfClosing;

    // 调用shutdown关闭读写 意味着服务器不会再对这个fd进行任何读写
    // 仅仅是发送了一个FIN，出发了四次挥手的第一个阶段
    // 当fd发生可读事件，但是可读事件的数据为0，表示对端发送的是FIN
    ::shutdown(m_fd, SHUT_RDWR);
}

void TcpConnection::setConnectionType(TcpConnectionType type) {
    m_connection_type = type;
}

void TcpConnection::listenWrite() {
    m_fd_event->listen(FdEvent::OUT_EVENT, std::bind(&TcpConnection::onWrite, this));
    m_event_loop->addEpollEvent(m_fd_event);
}

void TcpConnection::listenRead() {
    m_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpConnection::onRead, this));
    m_event_loop->addEpollEvent(m_fd_event);
}

void TcpConnection::pushSendMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done) {
    m_write_dones.push_back(std::make_pair(message, done));
}   

void TcpConnection::pushReadMessage(const std::string& req_id, std::function<void(AbstractProtocol::s_ptr)> done) {
    m_read_dones.insert(std::make_pair(req_id, done));
}

IPNetAddr::s_ptr TcpConnection::getLocalAddr() {
    return m_local_addr;
}

IPNetAddr::s_ptr TcpConnection::getPeerAddr() {
    return m_peer_addr;
}


}
