#include <string.h>
#include <fcntl.h>
#include "rocket/net/fd_event.h"
#include "rocket/common/log.h"

namespace rocket {

FdEvent::FdEvent(int fd) : m_fd(fd) {
    memset(&m_listen_events, 0, sizeof(m_listen_events));
}

FdEvent::FdEvent() {
    memset(&m_listen_events, 0, sizeof(m_listen_events));
}

FdEvent::~FdEvent() {


}


std::function<void()> FdEvent::handler(TriggerEvent event_type) {
    if (event_type == TriggerEvent::IN_EVENT) {
        return m_read_callback;
    }else if (event_type == TriggerEvent::OUT_EVENT) {
        return m_write_callback;
    } else if (event_type == TriggerEvent::ERROR_EVENT) {
        return m_error_callback;
    }
    return nullptr;
} 

//若监听到的事件是输入事件，则把回调函数注册到read上，否则注册到write上
void FdEvent::listen(TriggerEvent event_type, std::function<void()> callback, std::function<void()> error_callback/*=nullptr*/) {
    if (event_type == TriggerEvent::IN_EVENT) {
        m_listen_events.events |= EPOLLIN;
        m_read_callback = callback;
        m_listen_events.data.ptr = this; // 以便在 epoll_wait 返回时能够正确地获取与该文件描述符相关的对象。
                                         // 相当于是绑定了，每次监听到一个事件，就把fd_event的地址作为事件的data.ptr
    }else {
        m_listen_events.events |= EPOLLOUT;
        m_write_callback = callback;
        m_listen_events.data.ptr = this;
    }

    // if (error_callback != nullptr) {
    //     m_error_callback = error_callback;
    // } else {
    //     m_error_callback = nullptr; 
    // }
    
    if (m_error_callback == nullptr) {
        m_error_callback = error_callback;
    } else {
        m_error_callback = nullptr;
    }
}



void FdEvent::cancel(TriggerEvent event_type) {
    if (event_type == TriggerEvent::IN_EVENT) {
        m_listen_events.events &= (~EPOLLIN);
    }else {
        m_listen_events.events &= (~EPOLLOUT);
    }
}

void FdEvent::setNonBlock() {
    int flag = fcntl(m_fd, F_GETFL, 0);
    if (flag & O_NONBLOCK) {
        return;
    }

    fcntl(m_fd, F_SETFL, flag | O_NONBLOCK);
    
}

}