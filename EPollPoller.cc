#include "EPollPoller.h"
#include "Logger.h"
#include <unistd.h>
#include "Channel.h"
#include <memory.h>

const int kNew = -1; // Channel 未添加到 Poller 中， channel成员index_ = -1
const int kAdded = 1; // Channel 已添加到 Poller 中
const int kDeleted = 2; // Channel 从 Poller 中删除

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
    epollfd_(epoll_create1(EPOLL_CLOEXEC)),
    events_(kInitEventListSize){
        if (epollfd_ < 0){
            LOG_FATAL("epoll_create error:%d \n", errno);
        }
}

EPollPoller::~EPollPoller(){
    close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels){

}

// Channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
//            EventLoop
//     ChannelList     Poller
//                   ChannelMap  <fd, Channel*>
void EPollPoller::updateChannel(Channel* channel){
    const int index = channel->index();
    LOG_INFO("fd=%d events=%d index=%d \n", channel->fd(), channel->events(), index);
    if (index == kNew || index == kDeleted){
        int fd = channel->fd();
        if (index == kNew){
            channels_[fd] = channel;
        }else{
            channel->set_index(kAdded);
            update(EPOLL_CTL_ADD, channel);
        }
    }else{ // channel已经在poller上注册过了
        if(channel->isNoneEvent()){
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从Poller中删除channel
void EPollPoller::removeChannel(Channel* channel){
    int fd = channel->fd();
    channels_.erase(fd);

    int index = channel->index();
    if (index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPollPoller::fillActivateChannels(int numEvents, ChannelList *activeChannels) const{

}

void EPollPoller::update(int operation, Channel *channel){
    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();
    if (epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if (operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }else{
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}