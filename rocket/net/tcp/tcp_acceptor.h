#ifndef ROCKET_NET_TCP_TCP_ACCEPTOR_H
#define ROCKET_NET_TCP_TCP_ACCEPTOR_H

#include "rocket/net/tcp/net_addr.h"

namespace rocket {

class TcpAcceptor {

public:
    TcpAcceptor(NetAddr::s_ptr local_addr);

    ~TcpAcceptor();

    int accept();


    

private:
    // 服务端监听地址, addr -> ip:port
    NetAddr::s_ptr m_local_addr;

    // listenfd
    int m_listenfd {-1};

    int m_family {-1};


     



};

}


#endif