#include "TcpServer.h"
#include <string>
#include "Logger.h"
#include "Callbacks.h"

static EventLoop* CheckLoopNotNull(EventLoop* Loop){
    if (Loop == nullptr){
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return Loop;
}

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const std::string& nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    {
        // 当有新用户连接时，会执行TcpServer::newConnection回调
        acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, 
        this, std::placeholders::_1, std::placeholders::_2));
}

//设置底层subLoop的个数
void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start(){
    // 防止一个TcpServer对象被start多次
    if (started_++ == 0){
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }

}