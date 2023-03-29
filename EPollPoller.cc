#include "EPollPoller.h"
#include "Logger.h"
#include <unistd.h>
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


// Channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
//            EventLoop
//     ChannelList     Poller
//                   ChannelMap  <fd, Channel*>
void EPollPoller::updateChannel(Channel* channel){
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);
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

    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);
    int index = channel->index();
    if (index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
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

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels){
    // 用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());
    int numEvents = epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0){
        LOG_INFO("%d events happend \n", numEvents);
        fillActivateChannels(numEvents, activeChannels);
        if (numEvents == events_.size()){
            events_.resize(events_.size() * 2);
        }
    }else if (numEvents == 0){
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }else{
        if (saveErrno != EINTR){
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

void EPollPoller::fillActivateChannels(int numEvents, ChannelList *activeChannels) const{
    for (int i = 0; i < numEvents; ++i){
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了
    }
}