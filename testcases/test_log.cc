#include <pthread.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"


void* fun(void*) {
    for(int i=0; i<100; ++i) {
        DEBUGLOG("this is thread in debug %s", "fun");
        INFOLOG("this is thread in info %s", "fun");
    }    
    return NULL;
}


int main() {
    rocket::Config::SetGlobalConfig("./conf/rocket.xml");
    rocket::Logger::InitGlobalLogger();

    pthread_t thread;
    pthread_create(&thread, NULL, &fun, NULL);

    for(int i=0; i<100; ++i) {
        DEBUGLOG("test log %s", "debug");
        INFOLOG("test log %s", "info");
    }

    pthread_join(thread, NULL);

    return 0;
}