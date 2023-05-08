

#ifndef EPOLLER_H
#define EPOLLER_H

#include<sys/epoll.h>
#include<fcntl.h>
#include<unistd.h>      // close
#include<assert.h>      // close
#include<vector>
#include<errno.h>


class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);
    ~Epoller();

    bool addFd(int fd, uint32_t events);
    bool modFd(int fd, uint32_t events);
    bool delFd(int fd);

    int wait(int timeoutMs = -1);

    int getEventFd(size_t i) const;
    uint32_t getEvents(size_t i) const;

private:
    int epollFd_;                               // 保存 epoll 的 fd

    std::vector<epoll_event> events_;    // epoll事件 数组
};


#endif  // EPOLLER_H