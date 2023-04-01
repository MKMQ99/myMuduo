#include "TcpConnection.h"
#include "Logger.h"
#include <string>
#include "Channel.h"
#include "Socket.h"
#include <errno.h>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

static EventLoop* CheckLoopNotNull(EventLoop* Loop){
    if (Loop == nullptr){
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return Loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
                             const std::string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024) // 64M
{
    // 下面给Channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了， channel会回调相应的操作函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));
    LOG_INFO("TcpConnection::ctor[%s] fd=%d", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection(){
    LOG_INFO("TcpConnection::dtor[%s] fd=%d state=%d", name_.c_str(), channel_->fd(), (int)state_);
}

// 发送数据
void TcpConnection::send(const void* message, int len){

}

// 关闭连接
void TcpConnection::shutdown(){
    if (state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

// 连接建立
void TcpConnection::connectEstablished(){

}

// 连接销毁
void TcpConnection::connectDestroyed(){

}

void TcpConnection::handleRead(Timestamp receiveTime){
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0){
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }else if (n == 0){
        handleClose();
    }else{
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite(){
    if (channel_->isWriting()){
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0){
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0){
                channel_->disableReading();
                if (writeCompleteCallback_){
                    // 唤醒 loop_ 对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting){
                    shutdownInLoop();
                }
            }
        }else{
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }else{
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}

void TcpConnection::handleClose(){
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis); // 执行连接关闭的回调
    closeCallback_(guardThis);  // 关闭连接的回调
}

void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof(optval));
    int err = 0;

    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno;
    }else{
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

void TcpConnection::sendInLoop(const void* message, size_t len){

}

void TcpConnection::shutdownInLoop(){
    if (!channel_->isWriting()){
        socket_->shutdownWrite();
    }
}