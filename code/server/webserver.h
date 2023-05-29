

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include<unordered_map>
#include<fcntl.h>
#include<unistd.h>
#include<assert.h>
#include<errno.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include"epoller.h"
#include"../log/log.h"
#include"../timer/heaptimer.h"
#include"../pool/sqlconnpool.h"
#include"../pool/sqlconnRAII.hpp"
#include"../pool/threadpool.hpp"
#include"../http/httpconn.h"


class WebServer {
public:
    WebServer(
        // 监听端口，ET模式，timeoutMs，优雅退出
        // Mysql：端口，用户名，密码，数据库名
        // 连接池大小，线程池大小，日志开关、等级、异步队列容量
        int port, int trigMode, int timeoutMS, bool optLinger,
        int sqlPort, const char* sqlUser, const char* sqlPwd, const char* dbName, 
        int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize
    );
    ~WebServer();

    void start();

private:
    bool initSocket_();
    void initEventMode_(int trigMode);
    
    void addClient_(int fd, sockaddr_in addr);

    void dealListen_();
    void dealRead_(HttpConn* client);
    void dealWrite_(HttpConn* client);

    void sendError_(int fd, const char* info);
    void extentTime_(HttpConn* client);
    void closeConn_(HttpConn* client);

    void onRead_(HttpConn* client);
    void onWrite_(HttpConn* client);
    void onProcess_(HttpConn* client);

    static const int MAX_FD = 65536;            // 最大的文件描述符数

    static int setFdNonblock_(int fd);

    int port_;              // 监听端口
    int timeoutMS_;         // 超时时间
    bool openLinger_;       // 是否开启 优雅退出
    bool isClose_;          // 是否关闭服务器

    char* srcDir_;          // 记录服务器资源所在路径   .../resources
    int listenFd_;          // 记录监听的文件描述符

    uint32_t listenEvent_;  // 监听事件
    uint32_t connEvent_;    // 连接事件

    std::unique_ptr<HeapTimer> timer_;          // 定时器
    std::unique_ptr<ThreadPool> threadpool_;    // 线程池
    std::unique_ptr<Epoller> epoller_;          // epoll

    std::unordered_map<int, HttpConn> users_;   // 保存所有客户端连接，fd To HttpConn
};


#endif  // WEBSERVER_H