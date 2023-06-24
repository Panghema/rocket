#ifndef ROCKET_NET_TCP_TCP_BUFFER_H
#define ROCKET_NET_TCP_TCP_BUFFER_H

#include <vector>

namespace rocket
{

class TcpBuffer {

public:

    TcpBuffer(int size);

    ~TcpBuffer();

    // 可读的字节数
    int readAble();

    // 可写字节数
    int writeAble();

    int readIndex();

    int writeIndex();

    void writeToBuffer(const char* buf, int size);

    void readFromBuffer(std::vector<char>& re, int size);

    void resizeBuffer(int new_size);

    void adjustBuffer();

    // 手动跳过一些要读的数据
    void moveReadIndex(int size);

    // 从新下表开始写
    void moveWriteIndex(int size);


private:
    int m_read_index {0};
    int m_write_index {0};
    int m_size {0};

    std::vector<char> m_buffer;
};
    
} // namespace rocket


#endif