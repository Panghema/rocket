#include <sys/timerfd.h>
#include <string.h>
#include "rocket/net/timer.h"
#include "rocket/common/log.h"
#include "rocket/common/util.h"

namespace rocket {

Timer::Timer() : FdEvent() {

    // 设置一个timerfd， 到时间之后就会给这个fd一个读信号
    m_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    DEBUGLOG("timer fd=%d", m_fd);
    
    // 把fd的可读事件放到了eventloop上监听
    listen(FdEvent::IN_EVENT, std::bind(&Timer::onTimer, this));
    

}

Timer::~Timer() {

}

// 触发可读事件后，需要的回调函数
void Timer::onTimer() {

    char buf[8]; // 要清除缓冲区数据，我们是水平触发，下次直接触发了
    while(true) {
        if ((read(m_fd, buf, 8) == -1) && errno == EAGAIN) {
            break;
        }
    }

    // 执行定时任务
    int64_t now = getTimeStamp();

    std::vector<TimerEvent::s_ptr> tmps;
    std::vector<std::pair<int64_t, std::function<void()>>> tasks;

    ScopeMutex<Mutex> lock(m_mutex);
    auto it = m_pending_events.begin();


    // 遍历取出所有要执行的任务，到时间了的加入任务独立额
    for(;it!=m_pending_events.end(); ++it) {
        if ((*it).first <= now) {
            if (!(*it).second->isCancel()) {
                tmps.push_back((*it).second);
                tasks.push_back(std::make_pair((*it).second->getArriveTime(), (*it).second->getCallBack()));
            }
        } else {
            break;
        }
    }

    m_pending_events.erase(m_pending_events.begin(), it);
    lock.unlock();

    // 需要把重复的event事件再次添加
    // 目的就是重新排序他们
    for (auto i=tmps.begin(); i!=tmps.end(); ++i) {
        if ((*i)->isRepeated()) {
            // 调整arriveTime 
            (*i)->resetArriveTime();
            addTimerEvent(*i);
        }
    }

    resetArriveTime();

    for (auto i : tasks) {
        if (i.second) {
            i.second();
        }
    }


}

void Timer::resetArriveTime() {
    ScopeMutex<Mutex> lock(m_mutex);
    auto tmp = m_pending_events;
    lock.unlock();

    if (tmp.size() == 0) { // BUG:忘记判断等于0
        return;
    }

    int64_t now = getTimeStamp();

    auto it = tmp.begin();
    int64_t interval = 0;
    // 如果第一个定时任务的启动时间比现在晚，那就设置间隔为他们的差
    // 否则就设置100ms后触发可读时间，启动任务，以拯救过期任务
    if (it->second->getArriveTime() > now) {
        interval = it->second->getArriveTime() - now;
    } else {
        interval = 100;
    }

    timespec ts;
    memset(&ts, 0, sizeof(ts));
    ts.tv_sec = interval / 1000;
    ts.tv_nsec = (interval % 1000) * 1000000;

    itimerspec value;
    memset(&value, 0, sizeof(value));
    value.it_value = ts;

    int rt = timerfd_settime(m_fd, 0, &value, NULL);
    if (rt != 0) {
        ERRORLOG("timerfd_settime error, errno=%d, error=%s", errno, strerror(errno));
    }
    DEBUGLOG("timer reset to %lld", now + interval);
}

void Timer::addTimerEvent(TimerEvent::s_ptr event) {
    bool is_reset_timerfd = false;

    ScopeMutex<Mutex> lock(m_mutex);
    if (m_pending_events.empty()) {
        is_reset_timerfd = true;
    } else {
        auto it = m_pending_events.begin();
        // 新插入的定时任务比所有任务的定时时间早
        // 如果不修改，当前默认定时时间还是最小的那个（begin的）
        // 只有到了begin的才触发，所以要把最小的更新了
        if ((*it).second->getArriveTime() > event->getArriveTime()) {
            is_reset_timerfd = true;
        }
    }
    m_pending_events.emplace(event->getArriveTime(), event);
    lock.unlock();
    
    if (is_reset_timerfd) {
        resetArriveTime();
    }

}

void Timer::deleteTimerEvent(TimerEvent::s_ptr event) {
    event->setCancel(true);
    
    ScopeMutex<Mutex> lock(m_mutex);
    auto begin = m_pending_events.lower_bound(event->getArriveTime());
    auto end = m_pending_events.upper_bound(event->getArriveTime());

    auto it = begin;
    for (; it!=end; ++it) {
        if (it->second == event) {
            break;
        }
    }

    if (it != end) {
        m_pending_events.erase(it);
    }

    lock.unlock();
    DEBUGLOG("success delete TimerEvent at arrive time %lld", event->getArriveTime());

}


}