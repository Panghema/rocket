#include <string.h>
#include <rocket/net/tcp/tcp_buffer.h>
#include <rocket/common/log.h>

namespace rocket {

TcpBuffer::TcpBuffer(int size) : m_size(size) {
    m_buffer.resize(size);
}

TcpBuffer::~TcpBuffer() {
    
}

int TcpBuffer::readAble() {
    return m_write_index - m_read_index;
}

int TcpBuffer::writeAble() {
    return m_size - m_write_index; // diff
}

int TcpBuffer::readIndex() {
    return m_read_index;
}

int TcpBuffer::writeIndex() {
    return m_write_index;
}

void TcpBuffer::writeToBuffer(const char* buf, int size) {
    if (size > writeAble()) {
        // 调整buffer大小，扩容
        int new_size = int(1.5 * (m_write_index + size)); // 从m_write_index开始加，就是确保扩容后一定是够的，免得1.5*size没有m_write_size+size大
        resizeBuffer(new_size);
    }
    memcpy(&m_buffer[m_write_index], buf, size);
    m_write_index += size;
    
}

void TcpBuffer::resizeBuffer(int new_size) {
    std::vector<char> tmp(new_size);
    int count = std::min(new_size, readAble());

    memcpy(&tmp[0], &m_buffer[m_read_index], count);
    m_buffer.swap(tmp);

    m_read_index = 0;
    m_write_index = m_read_index + count;
    m_size = new_size;
}

void TcpBuffer::readFromBuffer(std::vector<char>& re, int size){
    if (readAble() == 0) {
        return;
    }

    int read_size = readAble() > size ? size : readAble();

    std::vector<char> tmp(read_size);
    memcpy(&tmp[0], &m_buffer[m_read_index], read_size);

    re.swap(tmp);
    m_read_index += read_size;

    adjustBuffer();

}

void TcpBuffer::adjustBuffer() {
    if (m_read_index < int(m_size / 3)) {
        return;
    }
    std::vector<char> buffer(m_size);
    int count = readAble();

    memcpy(&buffer[0], &m_buffer[m_read_index], count);
    m_buffer.swap(buffer);
    m_read_index = 0;
    m_write_index = m_read_index + count;

    buffer.clear();
}

void TcpBuffer::moveReadIndex(int size) {
    if (m_read_index + size >= size) {
        ERRORLOG("moveReadIndex error, invalid size %d, old_read_size %d, buffer size %d", size, m_read_index, m_size);
        return;
    }
    m_read_index = m_read_index + size;
    adjustBuffer();
}

void TcpBuffer::moveWriteIndex(int size){
    // DEBUGLOG("************** %d ***************", m_read_index)
    if (m_write_index + size >= m_size) {
        ERRORLOG("moveWriteIndex error, invalid size %d, old_read_size %d, buffer size %d", size, m_read_index, m_size);
        return;
    }
    m_write_index = m_write_index + size;
    adjustBuffer();
}

}


