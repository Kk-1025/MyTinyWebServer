

#include"epoller.h"


// 初始化 Epoller，maxEvent：事件数组大小
//explicit Epoller(int maxEvent = 1024);
Epoller::Epoller(int maxEvent)
    : epollFd_(epoll_create(512)), events_(maxEvent)
    // 512 仅为给内核参考的 epoll 树的监听节点数量
{
    assert(epollFd_ >= 0 && events_.size() > 0);
}


Epoller::~Epoller()
{
    close(epollFd_);
}


// 添加监听事件
bool Epoller::addFd(int fd, uint32_t events)
{
    // base case
    if (fd < 0) {
        return false;
    }

    // 初始化 epoll_event 结构体
    epoll_event ev = { 0 };
    ev.data.fd = fd;
    ev.events = events;

    // 将 监听事件 添加到 epoll树上
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}


// 修改监听事件
bool Epoller::modFd(int fd, uint32_t events)
{
    // base case
    if (fd < 0) {
        return false;
    }

    // 初始化 epoll_event 结构体
    epoll_event ev = { 0 };
    ev.data.fd = fd;
    ev.events = events;

    // 修改 epoll树上的监听事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}


// 删除 事件
bool Epoller::delFd(int fd)
{
    // base case
    if (fd < 0) {
        return false;
    }

    // 删除 epoll树上的监听事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, NULL);
}


// 等待监听事件发生
//int wait(int timeoutMs = -1);     // 默认为阻塞等待
int Epoller::wait(int timeoutMs)
{
    return epoll_wait(epollFd_, (epoll_event*)&events_[0], static_cast<int>(events_.size()), timeoutMs);
}


// 返回某个监听事件的 fd
int Epoller::getEventFd(size_t i) const
{
    assert(0 <= i && i < events_.size());

    return events_[i].data.fd;

}


// 返回某个监听事件的 events
uint32_t Epoller::getEvents(size_t i) const
{
    assert(0 <= i && i < events_.size());
    
    return events_[i].events;
}