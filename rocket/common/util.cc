#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include "rocket/common/util.h"

namespace rocket {

static int g_pid = 0;
static thread_local int g_thread_id = 0; // 使用thread_local关键字，是为了减少锁的使用，避免多个线程之间共享数据的问题。

pid_t getPid() {
    if (g_pid != 0) {
        return g_pid;
    }
    return getpid();
}

pid_t getThreadId() {
    if (g_thread_id != 0) {
        return g_thread_id;
    }
    return syscall(SYS_gettid);
}

int64_t getTimeStamp() {
    timeval val;
    gettimeofday(&val, NULL);

    return val.tv_sec * 1000 + val.tv_usec / 1000;
}

}

