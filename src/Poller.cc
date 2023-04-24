#include "Poller.h"

Poller::Poller(EventLoop *Loop): ownerLoop_(Loop){}

bool Poller::hasChannel(Channel *channel) const{
    ChannelMap::const_iterator it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}