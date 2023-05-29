

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include<sys/types.h>
#include<sys/uio.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<errno.h>

#include"../log/log.h"
#include"../pool/sqlconnRAII.hpp"
#include"../buffer/buffer.h"
#include"httprequest.h"
#include"httpresponse.h"


class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);
    void Close();

    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);

    int getFd() const;
    sockaddr_in getAddr() const;
    const char* getIP() const;
    int getPort() const;

    bool process();

    int toWriteBytes();
    bool isKeepAlive() const;

    static bool isET;                   // 是否为 ET边沿触发模式
    static const char* srcDir;          // 存放服务器资源文件的路径
    static std::atomic<int> userCount;  // 原子变量：记录连接的客户端数量

private:

    int fd_;
    struct sockaddr_in addr_;

    bool isClose_;

    int iovCnt_;                // iovec 数量
    struct iovec iov_[2];       // iovec：分散读、聚集写
    // iov_[0]存响应信息/写缓冲区，iov_[1]存资源文件/共享内存区

    Buffer readBuff_;           // 读缓冲区
    Buffer writeBuff_;          // 写缓冲区

    HttpRequest request_;       // 请求
    HttpResponse response_;     // 响应
};



#endif  // HTTP_CONN_H
