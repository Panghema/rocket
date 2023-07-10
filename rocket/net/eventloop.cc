#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <string.h>
#include "rocket/net/eventloop.h"
#include "rocket/common/log.h"
#include "rocket/common/util.h"

// 每个线程独立拥有一个epoll 去监听EPOLLIN EPOLLOUT 并使用对应的回调函数


#define ADD_TO_EPOLL() \
    auto it = m_listen_fds.find(event->getFd()); \
    int op = EPOLL_CTL_ADD; \
    if(it != m_listen_fds.end()) { \
        op = EPOLL_CTL_MOD; \
    } \
    epoll_event tmp = event->getEpollEvent(); \
    int rt = epoll_ctl(m_epoll_fd, op, event->getFd(), &tmp); \
    if (rt == -1) { \
        ERRORLOG("failed epoll_ctl when add fd %d, errno=%d, err info=%s", event->getFd(), errno, strerror(errno)); \
    } \
    m_listen_fds.insert(event->getFd()); \
    DEBUGLOG("add event success, fd[%d]", event->getFd()); \

#define DELETE_FROM_EPOLL() \
    auto it = m_listen_fds.find(event->getFd()); \
    if(it == m_listen_fds.end()) { \
        return; \
    } \
    int op = EPOLL_CTL_DEL; \
    epoll_event tmp = event->getEpollEvent(); \
    int rt = epoll_ctl(m_epoll_fd, op, event->getFd(), &tmp); \
    if (rt == -1) { \
        ERRORLOG("failed epoll_ctl when delete fd %d, errno=%d, err info=%s", event->getFd(), errno, strerror(errno)); \
    } \
    m_listen_fds.erase(event->getFd()); \
    DEBUGLOG("delete event success, fd[%d]", event->getFd()); \


namespace rocket {

static thread_local EventLoop* t_current_eventloop = NULL;
static int g_epoll_max_timeout = 10000;
static int g_epoll_max_events = 10;

EventLoop::EventLoop() {
    if (t_current_eventloop != NULL) { // 没有创建过
        ERRORLOG("%s", "Failed to create event loop, this thread has created event loop");
        exit(0);
    }

    m_pthread_id = getThreadId();

    m_epoll_fd = epoll_create(10);
    if(m_epoll_fd == -1) {
        ERRORLOG("Failed to create event loop, epoll_create error, error info[%s]", errno);
        exit(0);
    }

    initWakeUpFdEvent();
    initTimer();
    INFOLOG("succ create eventloop in thread %d", m_pthread_id);
    t_current_eventloop = this;
}

EventLoop::~EventLoop() {
    close(m_epoll_fd);
    if (m_wakeup_fd_event) {
        delete m_wakeup_fd_event;
        m_wakeup_fd_event = NULL;
    }

    if (m_timer) {
        delete m_timer;
        m_timer = NULL;
    }
}

void EventLoop::initTimer() {
    m_timer = new Timer();
    addEpollEvent(m_timer);
}

void EventLoop::addTimerEvent(TimerEvent::s_ptr event) {
    m_timer->addTimerEvent(event);
}

void EventLoop::initWakeUpFdEvent() {
    m_wakeup_fd = eventfd(0, EFD_NONBLOCK);
    if (m_wakeup_fd < 0) {
        ERRORLOG("Failed to create event loop, eventfd error, error info[%s]", errno);
        exit(0);
    }

    m_wakeup_fd_event = new WakeUpFdEvent(m_wakeup_fd); // 将m_wakeup_fd注册到event，和一个event绑定，这个event的回调函数有下一行决定，
                                                        // 并且该wakeupevent类的具体的event的地址也注册为该对象地址
    m_wakeup_fd_event->listen(FdEvent::IN_EVENT, [this]() { // 启动监听，若检测到wakeup时间收到EPOLLIN事件，则调用回调函数
        char buf[8];
        while(read(m_wakeup_fd, buf, 8) != -1 && errno != EAGAIN) {}
        DEBUGLOG("read full bytes from wakeup fd[%d]", m_wakeup_fd);

    });

    addEpollEvent(m_wakeup_fd_event); // 把在注册好修改指向对象和ptr等属性的event注册进去，以后对该event的访问（通过ptr）都会指向定义的fd_event类中
}

void EventLoop::loop() {
    m_is_looping = true;
    while(!m_stop_flag) {
        // 先取出所有任务
        ScopeMutex<Mutex> lock(m_mutex);
        std::queue<std::function<void()>> tmp_tasks;
        m_pending_tasks.swap(tmp_tasks);
        lock.unlock();
        // 先执行任务再epoll_wait是为了防止一开始就有任务进来，但epoll还没开始，刚开始的任务就只能等到epoll开始才能执行了。
        while(!tmp_tasks.empty()) {
            std::function<void()> cb = tmp_tasks.front();
            tmp_tasks.pop();
            if (cb) {
                cb();
            }
        }

        // 如果有定时任务需要执行，那么
        // 1.怎么判断一个定时任务需要执行？（now（）>TimerEvent.arrive_time）
        // 2.arrive_time 如何让eventloop做到监听

        int timeout = g_epoll_max_timeout;
        epoll_event result_events[g_epoll_max_events];
        // DEBUGLOG("now begin to epoll_wait");
        int rt = epoll_wait(m_epoll_fd, result_events, g_epoll_max_events, timeout); // 在刚开始只有wakeup fd被添加的时候，其实也只能监听到wakeupfd

        DEBUGLOG("now end epoll_wait, rt = %d", rt);
        if(rt < 0) {
            ERRORLOG("epoll_wait error, errono=%d", errno);
        }else {
            for(int i=0; i<rt; ++i) {
                
                epoll_event trigger_event = result_events[i];
                FdEvent* fd_event = static_cast<FdEvent*>(trigger_event.data.ptr); // 对应FdEvent::listen 中的m_listen_events.data.ptr = this;
                                                                                   // 通过之前注册的对应关系，这里拿到的data.ptr其实就是fd_event的地址
                if (fd_event == NULL) {
                    continue;
                }
                if (trigger_event.events & EPOLLIN) {

                    DEBUGLOG("fd %d trigger EPOLLIN event", fd_event->getFd())
                    addTask(fd_event->handler(FdEvent::IN_EVENT));
                }
                if (trigger_event.events & EPOLLOUT) {

                    DEBUGLOG("fd %d trigger EPOLLOUT event", fd_event->getFd())
                    addTask(fd_event->handler(FdEvent::OUT_EVENT));
                }
            }
        }
    }
}

void EventLoop::wakeup() {
    m_wakeup_fd_event->wakeup();
}

void EventLoop::stop() {
    m_stop_flag = true;
}

void EventLoop::dealWakeup() {
    
}

// 用封装好的类去控制所有的fd，每个对象中包括了fd对应的event，回调函数等。
void EventLoop::addEpollEvent(FdEvent* event) {
    if (isInLoopThread()) {
        ADD_TO_EPOLL();
    } else {
        auto cb = [this, event]() {
            ADD_TO_EPOLL();
        };
        addTask(cb, true);
    }
}

// 如果判断是在操作本线程的epoll的话，那就直接操作（增、删）
// 如果不是在操作本线程的epoll的话，就把要进行的操作，封装成回调函数，加入到任务队列
// 等到后面loop重新捕获该任务了，又可以进行操作。
void EventLoop::deleteEpollEvent(FdEvent* event) {
    if (isInLoopThread()) {
        DELETE_FROM_EPOLL();
    } else {
        auto cb = [this, event]() {
            DELETE_FROM_EPOLL();
        };
        addTask(cb, true);
    }
}

// is_wake_up表示是否紧急，如果是，则迅速唤醒epoll，不等wait自然结束
void EventLoop::addTask(std::function<void()> cb, bool is_wake_up) {
    ScopeMutex<Mutex> lock(m_mutex);
    m_pending_tasks.push(cb);
    lock.unlock();
    if(is_wake_up) {
        wakeup();
    }
}

bool EventLoop::isInLoopThread() {
    pthread_t thread_id = getThreadId();
    return thread_id == m_pthread_id;
}

EventLoop* EventLoop::getCurrentEventLoop() {
    if (t_current_eventloop) {
        return t_current_eventloop;
    }
    t_current_eventloop = new EventLoop();
    return t_current_eventloop;
}

bool EventLoop::isLooping() {
    return m_is_looping;
}

}