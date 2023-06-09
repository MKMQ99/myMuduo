#pragma once
#include "noncopyable.h"
#include <memory>
#include <string>
#include <atomic>
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"
#include "EventLoop.h"
#include <any>

class Channel;
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

        EventLoop* getLoop() const { return loop_; }
        const std::string& name() const { return name_; }
        const InetAddress& localAddress() const { return localAddr_; }
        const InetAddress& peerAddress() const { return peerAddr_; }
        
        bool connected() const { return state_ == kConnected; }
        bool disconnected() const { return state_ == kDisconnected; }

        // 发送数据
        void send(const std::string &buf);
        void send(Buffer* message);
        // 关闭连接
        void shutdown();

        void setContext(const std::any& context){ context_ = context; }

        std::any* getMutableContext() { return &context_; }

        void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = cb; }
        void setMessageCallback(const MessageCallback& cb){ messageCallback_ = cb; }
        void setWriteCompleteCallback(const WriteCompleteCallback& cb){ writeCompleteCallback_ = cb; }
        void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark){ highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }
        void setCloseCallback(const CloseCallback& cb){ closeCallback_ = cb; }

        // 连接建立
        void connectEstablished();
        // 连接销毁
        void connectDestroyed();

    private:
        enum StateE  { kDisconnected, kConnecting, kConnected, kDisconnecting };
        
        void handleRead(Timestamp receiveTime);
        void handleWrite();
        void handleClose();
        void handleError();

        void sendInLoop(const void* data, size_t len);
        void shutdownInLoop();

        void setState(StateE s) { state_ = s; }

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

        Buffer inputBuffer_; // 接收数据的缓冲区
        Buffer outputBuffer_; // 发送数据的缓冲区
        std::any context_;
};