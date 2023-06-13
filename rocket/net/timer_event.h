#ifndef ROCKET_NET_TIMEREVENT_H
#define ROCKET_NET_TIMEREVENT_H

#include <functional>
#include <memory>

namespace rocket {

class TimerEvent {

public:
    typedef std::shared_ptr<TimerEvent> s_ptr;
    TimerEvent(int interval, bool is_repeated, std::function<void()> cb);

    int64_t getArriveTime() {
        return m_arrive_time;
    }

    void setCancel(bool value) {
        m_is_cancel = value;
    }

    bool isCancel() {
        return m_is_cancel;
    }

    bool isRepeated() {
        return m_is_repeated;
    }

    std::function<void()> getCallBack() {
        return m_task;
    }

    void resetArriveTime();

private:
    int64_t m_arrive_time; // ms
    int64_t m_interval; // ms
    bool m_is_repeated {false};
    bool m_is_cancel {false};

    std::function<void()> m_task;

};



}

#endif