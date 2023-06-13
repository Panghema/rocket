#include <string.h>
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
    }else {
        return m_write_callback;
    }

} 

//若监听到的事件是输入事件，则把回调函数注册到read上，否则注册到write上
void FdEvent::listen(TriggerEvent event_type, std::function<void()> callback) {
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
}



}