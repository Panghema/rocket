#include "rocket/net/io_thread_group.h"
#include "rocket/common/log.h"


namespace rocket {

IOThreadGroup::IOThreadGroup(int size) : m_size(size) {
    m_io_thread_group.resize(size);

    for (int i=0; i<size; ++i) {
        m_io_thread_group[i] = new IOThread();
    }
}

IOThreadGroup::~IOThreadGroup() {

}

void IOThreadGroup::start() {
    for (size_t i=0; i<m_io_thread_group.size(); ++i) {
        m_io_thread_group[i]->start();
        // join ??
    }
}

void IOThreadGroup::join() {
    for (size_t i=0; i<m_io_thread_group.size(); ++i) {
        m_io_thread_group[i]->join();
    }
}

IOThread* IOThreadGroup::getIOThread() {
    if (size_t(m_index) == m_io_thread_group.size() || m_index == -1) {
        m_index = 0;
    }
    return m_io_thread_group[m_index++];
}




}