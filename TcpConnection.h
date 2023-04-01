#pragma once
#include "noncopyable.h"
#include <memory>
#include <string>
#include <atomic>
#include <InetAddress.h>
#include "Callbacks.h"
#include "Buffer.h"

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection 设置回调 => Channel 设置回调 => Poller => Channel进行回调
*/

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>{
    public:
        TcpConnection(EventLoop* loop
                , const std::string& name
                , int sockfd
                , const InetAddress& localAddr
                , const InetAddress& peerAddr);
        ~TcpConnection();

        
    private:
        enum StateE  { kDisconnected, kConnecting, kConnected, kDisconnecting };
        EventLoop *loop_; // 这里绝对不是baseLoop，因为TcpConnection都是在subLoop里管理的
        const std::string name_;
        std::atomic_int state_;
        bool reading_;

        std::unique_ptr<Socket> socket_;
        std::unique_ptr<Channel> channel_;

        const InetAddress localAddr_;
        const InetAddress peerAddr_;

        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;
        WriteCompleteCallback writeCompleteCallback_;
        HighWaterMarkCallback highWaterMarkCallback_;
        CloseCallback closeCallback_;

        size_t highWaterMark_;
        Buffer inputBuffer_;
        Buffer outputBuffer_;
};