#pragma once
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"
#include <functional>

class EventLoop;
class InetAddress;


class Acceptor : noncopyable{
    public:
        typedef std::function<void (int sockfd, const InetAddress&)> NewConnectionCallback;

        Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
        ~Acceptor();

        void setNewConnectionCallback(const NewConnectionCallback& cb){ newConnectionCallback_ = cb; }

        void listen();
        bool listening() const { return listenning_; }
    private:
        void handleRead();

        EventLoop *loop_; // Acceptor用的就是用户定义的那个baseLoop，也称作mainLoop
        Socket acceptSocket_;
        Channel acceptChannel_;
        NewConnectionCallback newConnectionCallback_;
        bool listenning_;
        int idleFd_;
};