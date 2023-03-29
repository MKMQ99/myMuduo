#include "Poller.h"
#include <stdlib.h>
Poller* Poller::newDefaultPoller(EventLoop *Loop){
    if (getenv("MUDUO_USE_POLL")){
        return nullptr; //生成poll实例
    }else{
        return nullptr; //生成epoll实例
    }
}