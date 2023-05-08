

#include"httpconn.h"
using namespace std;


// 静态变量的类外声明
bool HttpConn::isET;
const char* HttpConn::srcDir;
atomic<int> HttpConn::userCount;


HttpConn::HttpConn()
{
    fd_ = -1;
    addr_ = { 0 };

    isClose_ = true;
}


HttpConn::~HttpConn()
{
    Close();
}


// 初始化
void HttpConn::init(int sockFd, const sockaddr_in& addr)
{
    assert(sockFd > 0);

    ++userCount;    // 静态变量，记录总客户端连接数

    fd_ = sockFd;
    addr_ = addr;

    isClose_ = false;
    
    // 初始化 读写缓冲区
    readBuff_.retrieveAll();
    writeBuff_.retrieveAll();

    LOG_INFO("Client[%d](%s:%d) in, UserCount:%d", fd_, getIP(), getPort(), static_cast<int>(userCount));
}


// 关闭当前客户端连接
void HttpConn::Close()
{
    // 解除内存区映射
    response_.unmapFile();

    if (isClose_ == false) {
        isClose_ = true;

        --userCount;

        close(fd_);

        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, getIP(), getPort(), static_cast<int>(userCount));
    }
}


// 读取数据，保存在读缓冲区中，传出参数 errno
ssize_t HttpConn::read(int* saveErrno)
{
    ssize_t len = -1;

    do {
        len = readBuff_.readFd(fd_, saveErrno);

        if (len <= 0) {
            break;
        }
    } while (isET);     // 如果是ET模式，则一直读 直到读完数据；LT模式则只读一次

    return len;
}


// 写入数据，传出参数 errno
ssize_t HttpConn::write(int* saveErrno)
{
    ssize_t len = -1;

    do {
        // 聚集写，将 iovCnt_ 个 iov_ 内的数据 写入到 fd_ 中
        len = writev(fd_, iov_, iovCnt_);

        if (len <= 0) {
            *saveErrno = errno;     // 保存 writev 的 errno
            break;
        }

        // iov_[0]为写缓冲区中的响应信息，iov_[1]为共享内存区中的响应资源文件
        if (iov_[0].iov_len + iov_[1].iov_len == 0) {               // 两个 iovec 大小都为零，无数据要写入/写完
            break;
        }
        else if (static_cast<size_t>(len) > iov_[0].iov_len) {      // 第一个 iovec 的数据已写完，第二个不知道写完没

            // iov_[1] 更新 至未写入数据的位置
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);  // 为什么要转换指针？？？
            //iov_[1].iov_base = iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);

            // iov_[0]已写完，可以清空 iov_[0]
            if (iov_[0].iov_len) {
                writeBuff_.retrieveAll();   // iov_[0]映射的是写缓冲区，清空写缓冲区
                iov_[0].iov_len = 0;
            }
        }
        else {                                                      // 第一个 iovec 的数据没写完，第二个没写

            // 更新 iov_[0] 至未写入位置
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            //iov_[0].iov_base = iov_[0].iov_base + len;
            iov_[0].iov_len -= len;

            writeBuff_.retrieve(len);       // 更新写缓冲区
        }

    } while (isET || toWriteBytes() > 10240);   // ET模式：一直写 直到写完；或待写入数据过多，写到待写入数据不那么多

    return len;
}


// 获取 fd_
int HttpConn::getFd() const
{
    return fd_;
}


// 获取 addr_
sockaddr_in HttpConn::getAddr() const
{
    return addr_;
}


// 获取 IP
const char* HttpConn::getIP() const
{
    return inet_ntoa(addr_.sin_addr);
}


// 获取 port_
int HttpConn::getPort() const
{
    return addr_.sin_port;
}


// 解析 http 请求，生成 http 响应
bool HttpConn::process()
{
    // 初始化 请求 对象
    request_.init();

    if (readBuff_.readableBytes() <= 0) {   // 读缓冲区 没有可读取的数据
        return false;
    }
    else if (request_.parse(readBuff_)) {   // 读缓冲区中的数据，解析请求信息
        LOG_DEBUG("Request resource path: %s", request_.path().c_str());

        // 初始化 响应，200成功
        response_.init(srcDir, request_.path(), request_.isKeepAlive(), 200);
    }
    else {                                  // 解析请求失败，不是有效请求
        // 初始化 响应，400失败
        response_.init(srcDir, request_.path(), false, 400);
    }

    // 生成响应信息
    response_.makeResponse(writeBuff_);

    // 将响应信息 映射到 iov_[0]
    iov_[0].iov_base = const_cast<char*>(writeBuff_.peek());
    iov_[0].iov_len = writeBuff_.readableBytes();
    iovCnt_ = 1;

    // 将共享内存区/资源文件 映射到 iov_[1]
    if (response_.fileLen() > 0 && response_.file()) {
        iov_[1].iov_base = response_.file();
        iov_[1].iov_len = response_.fileLen();
        iovCnt_ = 2;
    }

    // ???
    //LOG_DEBUG("filesize:%d, %d to %d", response_.fileLen(), iovCnt_, toWriteBytes());
    LOG_DEBUG("%s, Response info size: %d, Resources size: %d, total: %d",
            response_.path().c_str(), iov_[0].iov_len, response_.fileLen(), toWriteBytes());

    return true;
}


// 返回 待写入的字节数，即 iov_ 总长度
int HttpConn::toWriteBytes() {

    return iov_[0].iov_len + iov_[1].iov_len;
}


// 检查是否仍与客户端保持连接
bool HttpConn::isKeepAlive() const {

    return request_.isKeepAlive();
}