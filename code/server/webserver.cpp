

#include"webserver.h"
using namespace std;

WebServer::WebServer(
        // 监听端口，ET模式，timeoutMs，优雅退出
        // Mysql：端口，用户名，密码，数据库名
        // 连接池数量，线程池数量，日志开关、等级、异步队列容量
        int port, int trigMode, int timeoutMS, bool optLinger,
        int sqlPort, const char* sqlUser, const char* sqlPwd, const char* dbName, 
        int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize)
        
        : port_(port), timeoutMS_(timeoutMS), openLinger_(optLinger), isClose_(false),
        timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    // 生成资源所在的路径
    srcDir_ = getcwd(nullptr, 256);     // getcwd：返回当前工作目录的绝对路径
    assert(srcDir_);
    
    strncat(srcDir_, "/resources", 16);    // 连接字符串，将 src 的前n个字符追加到 dest 后

    // 初始化 HttpConn 的静态成员
    HttpConn::srcDir = srcDir_;
    HttpConn::userCount = 0;

    // 初始化 SqlConnPool 的静态成员
    // 主机IP，端口，用户名，密码，数据库名，连接数
    SqlConnPool::instance()->init("127.0.0.1", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    // 初始化 ET 模式
    initEventMode_(trigMode);

    // 初始化 socket
    if (!initSocket_()) {
        isClose_ = true;   // 初始化失败 则关闭服务器
    }

    // 初始化日志系统
    if (openLog) {
        // 日志等级，存放路径，文件后缀，异步队列容量
        Log::instance()->init(logLevel, "./log", ".log", logQueSize);
        
        if (isClose_) {     // 初始化失败，关闭服务器
            LOG_ERROR("======== Server init error! ========");
        }
        else {
            LOG_INFO("======== Server init =======");
            LOG_INFO("Port:%d, OpenLinger:%s", port_, optLinger ? "true" : "false");    // 打印端口、是否优雅关闭
            LOG_INFO("Listen Mode:%s, OpenConn Mode:%s",                                // 打印监听/客户连接模式 ET/LT
                        (listenEvent_ & EPOLLET ? "ET" : "LT"),
                        (connEvent_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("LogSys level:%d", logLevel);                                      // 打印日志等级
            LOG_INFO("srcDir:%s", srcDir_);                                             // 打印资源路径
            LOG_INFO("SqlConnPool num:%d, ThreadPool num:%d", connPoolNum, threadNum);  // 打印 Mysql连接数、线程池线程数
        }
    }
}


WebServer::~WebServer()
{
    close(listenFd_);                       // 关闭监听端口
    isClose_ = true;                        // 设置服务器关闭
    free(srcDir_);                          // free掉 指针指向的空间
    SqlConnPool::instance()->closePool();   // 关闭 sql 连接池
}


// 开启服务器程序
void WebServer::start()
{
    int timeMS = -1;    // epoll_wait：timeout == -1，无事件时将阻塞
    
    // 打印日志
    if (!isClose_) {
        LOG_INFO("======== Server start ========");
    }

    // 只要服务器正常运行
    while (!isClose_) {

        // 开启计时器
        if (timeoutMS_ > 0) {
            timeMS = timer_->getNextTick();
        }

        // 处理事件
        int eventCnt = epoller_->wait(timeMS);  // timeMS：-1阻塞，0不阻塞，>0超时时间
        for (int i = 0; i < eventCnt; ++i) {
            int fd = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);
            
            if (fd == listenFd_) {                                      // 解决监听事件
                dealListen_();
            }
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {     // 检测到对端关闭
                assert(users_.count(fd) > 0);   // 先看看是否存在该fd
                closeConn_(&users_[fd]);
            }
            else if (events & EPOLLIN) {                                // 有数据到来，读事件
                assert(users_.count(fd) > 0);
                dealRead_(&users_[fd]);
            }
            else if (events & EPOLLOUT) {                               // 有数据要写，写事件
                assert(users_.count(fd) > 0);
                dealWrite_(&users_[fd]);
            }
            else {                                                      // 出错，不支持的事件发生
                LOG_ERROR("Unexpected epoll event!");
            }
        }
    }
}


// 初始化 socket
bool WebServer::initSocket_()
{
    // 检测端口范围是否合理
    if (port_ > 65535 || port_ < 1024) {    // 设置的监听端口 超出正常范围
        LOG_ERROR("Port:%d error! Invalid port range.", port_);
        return false;
    }

    // Socket
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);    // 获取监听文件描述符
    if (listenFd_ < 0) {                            // socket失败
        LOG_ERROR("Create socket error!");
        return false;
    }

    // 设置 优雅关闭/强制关闭
    struct linger optLinger = { 0 };
    if (openLinger_) {              // 设置了优雅关闭
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;     // 允许优雅关闭的时间为 1s，若 1s 内未发送完数据 就通过 RST包 强制关闭
    }
    int ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, (const void*)&optLinger, sizeof(optLinger));
    if (ret < 0) {
        LOG_ERROR("Init linger error!");
        close(listenFd_);
        return false;
    }

    // 设置端口复用，只有最后一个套接字会正常接收数据
    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {        // 为啥其他地方写<0 这里又 == -1了？？？
        LOG_ERROR("Setsockopt reuseaddr error!");
        close(listenFd_);   // 关闭已经创建好的 监听fd
        return false;
    }

    // 初始化 地址结构体
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    // Bind
    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // Listen
    ret = listen(listenFd_, 6);     // 可同时监听6个客户端的连接请求
    if (ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 将监听事件添加到 epoll
    ret = epoller_->addFd(listenFd_, listenFd_ | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR("Epoll add listen fd error!");
        close(listenFd_);
        return false;
    }

    // 设置 监听fd 为非阻塞
    setFdNonblock_(listenFd_);

    LOG_INFO("Init Socket success! Server port:%d", port_);  // 打印成功日志

    return true;
}


// 初始化事件的 ET/LT模式
void WebServer::initEventMode_(int trigMode)
{
    listenEvent_ = EPOLLRDHUP;              // 对端关闭时 会被触发
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; // 保证只有一个线程操作 fd

    switch (trigMode)               // 监听/连接事件 的模式
    {
    case 0:                         // 0:LT/LT
        break;
    case 1:                         // 1:LT/ET
        connEvent_ |= EPOLLET;
        break;
    case 2:                         // 2:ET/LT
        listenEvent_ |= EPOLLET;
        break;
    case 3:                         // 3:ET/ET
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:                        // 其他:ET/ET
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }

    // 初始化 HttpConn 的静态成员变量
    HttpConn::isET = (connEvent_ & EPOLLET);    // 传入 连接事件是否为 ET
}


// 添加客户端连接
// 将新客户端的 fd、addr 添加进来
void WebServer::addClient_(int fd, sockaddr_in addr)
{
    assert(fd > 0);

    // 添加到 users_ 哈希表中
    users_[fd].init(fd, addr);

    // 如果有设置 超时时间，设置计时器，到期就 关闭客户端连接
    if (timeoutMS_ > 0) {
        // 回调函数为 bind：WebServer this->closeConn_(&users_[fd]);
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::closeConn_, this, &users_[fd]));
    }

    // 添加到 epoll
    epoller_->addFd(fd, EPOLLIN | connEvent_);

    // 设置为非阻塞
    setFdNonblock_(fd);

    //LOG_INFO("Client[%d] in!", users_[fd].getFd());   // 通过调用 getFd() 检查 HttpConn 对象是否正常创建？
    LOG_INFO("Client[%d] in!", fd);
}


// 解决监听事件
void WebServer::dealListen_()
{
    // 新客户端的地址
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    do {
        // Accept
        int fd = accept(listenFd_, (struct sockaddr*)&addr, &len);      // listenFd_ 已被设置为非阻塞
        if (fd <= 0) {  // 失败直接返回
            return;
        }
        else if (HttpConn::userCount >= MAX_FD) {   // 达到最大的客户端容量
            sendError_(fd, "Server busy!");         // 给客户端发送繁忙信息

            LOG_WARN("Clients is full!");           // 打印日志
            return;
        }

        addClient_(fd, addr);           // 正常添加客户端

    } while (listenEvent_ & EPOLLET);   // 如果 监听事件 处于ET模式，一次性处理掉所有新连接请求
}


// 解决读事件
void WebServer::dealRead_(HttpConn* client)
{
    assert(client);

    // 更新连接到期时间
    extentTime_(client);

    // 线程池添加 读任务
    threadpool_->addTask(std::bind(&WebServer::onRead_, this, client));
}


// 解决写事件
void WebServer::dealWrite_(HttpConn* client)
{
    assert(client);

    // 更新连接到期时间
    extentTime_(client);

    // 线程池添加 写任务
    threadpool_->addTask(std::bind(&WebServer::onWrite_, this, client));
}


// 发送错误信息给客户端
// 客户端 fd，错误信息 info
void WebServer::sendError_(int fd, const char* info)
{
    assert(fd > 0);

    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("Send error to client[%d] error!", fd);
    }

    close(fd);      // 发送错误信息后 关闭客户端
}


// 更新连接到期时间
void WebServer::extentTime_(HttpConn* client)
{
    assert(client);

    if (timeoutMS_ > 0) {   // 只要设置了超时时间就更新
        timer_->adjust(client->getFd(), timeoutMS_);
    }
}


// 关闭与客户端的连接
void WebServer::closeConn_(HttpConn* client)
{
    assert(client);

    LOG_INFO("Client[%d] quit!", client->getFd());

    epoller_->delFd(client->getFd());               // epoll 删除节点
    client->Close();                                // 关闭连接
}


// 完成 读事件 的操作，交给线程池线程完成
void WebServer::onRead_(HttpConn* client)
{
    assert(client);

    int readErrno = 0;
    int ret = client->read(&readErrno);         // 读取客户端发来的数据，保存在缓冲区中 
    if (ret <= 0 && readErrno != EAGAIN) {      // 关闭客户端连接
        closeConn_(client);
        return;
    }

    // 处理客户端请求
    onProcess_(client);
}


// 完成 写事件 的操作，交给线程池线程完成
void WebServer::onWrite_(HttpConn* client)
{
    assert(client);

    int writeErrno = 0;
    int ret = client->write(&writeErrno);   // 客户端写入信息，并返回结果
    if (client->toWriteBytes() == 0) {      // 传输完成
        if (client->isKeepAlive()) {        // 客户端仍保持连接
            onProcess_(client);             // 缓冲区有数据 就继续处理客户端请求，否则将事件改为读事件 监听下一次请求
            return;
        }
    }
    else if (ret < 0) {
        if (writeErrno == EAGAIN) {     // 传输中断，暂不可写
            epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);    // 继续传输，重新注册为 写事件
            return;
        }
    }

    // 遇到问题，关闭客户端连接（传输完成且客户端已断开连接，或传输失败）
    closeConn_(client);
}


// 处理保存在缓冲区中的客户端请求 并将事件改为写事件；若缓冲区无内容 则继续保持读事件
void WebServer::onProcess_(HttpConn* client)
{
    if (client->process()) {    // 成功解析请求，并生成响应信息
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);    // 重新注册为 写事件
    } else {                    // 缓冲区不可读，失败
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN);     // 保持 读事件，监听客户端下一次请求
    }
}


// 设置 fd 为非阻塞
int WebServer::setFdNonblock_(int fd)
{
    assert(fd > 0);

    //return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}