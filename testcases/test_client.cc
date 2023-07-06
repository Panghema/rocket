#include <pthread.h>
#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <memory>
#include <unistd.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"

void test_connect() {
    // 调用connect连接server
    // write一个字符串
    // 等read返回结果

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ERRORLOG("invalid listenfd %d", fd);
        exit(0);
    }

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    inet_aton("127.0.0.1", &server.sin_addr);
    server.sin_port = htons(12345);

    int rt = connect(fd, reinterpret_cast<sockaddr*>(&server), sizeof(server));

    std::string msg = "hello rocket!";

    rt = write(fd, msg.c_str(), msg.size());

    DEBUGLOG("success write %d bytes, [%s]", rt, msg.c_str());

    char buf[128];
    rt = read(fd, buf, 128);

    DEBUGLOG("success read %d bytes, [%s]", rt, std::string(buf).c_str());

}

int main() {
    rocket::Config::SetGlobalConfig("./conf/rocket.xml");
    rocket::Logger::InitGlobalLogger();

    test_connect();

    return 0;
}