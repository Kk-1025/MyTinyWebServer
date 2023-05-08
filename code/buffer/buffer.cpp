

#include"buffer.h"


//Buffer(int initBuffSize = 1024);
Buffer::Buffer(int initBuffSize)
    :buffer_(initBuffSize), readPos_(0), writePos_(0)
{}


// 返回可读取的字节数
size_t Buffer::readableBytes() const
{
    return writePos_ - readPos_;
}


// 返回可写入的字节数
size_t Buffer::writableBytes() const
{
    return buffer_.size() - writePos_;
}


// 返回已读取的字节数，即当前 读位置
size_t Buffer::prependableBytes() const
{
    return readPos_;
}


// 返回当前读取到的位置 指针
const char* Buffer::peek() const
{
    return beginPtr_() + readPos_;
}


// 确保可写入 len 长度的数据，空间不够则拓展空间
void Buffer::ensureWritable(size_t len)
{
    if (writableBytes() < len) {    // 可写入的字节数 不够
        makeSpace_(len);            // 拓展空间
    }

    assert(writableBytes() >= len);
}


// 已写入 len 字节，写指针 往后移动 len
void Buffer::hasWritten(size_t len)
{
    // 此处不需要越界检查，在写入操作前 已执行 确保能写入的操作
    
    writePos_ += len;
}


// 已读取 len 字节，读指针 往后移动 len
void Buffer::retrieve(size_t len)              // 检索
{
    assert(len <= readableBytes());     // 确保读取没有越界

    readPos_ += len;
}


// 已读取数据 直到 end 位置，即读指针 往后移动 直到end
void Buffer::retrieveUntil(const char* end)    // 检索直到
{
    assert(peek() <= end);

    retrieve(end - peek());
}


// 已读取完全部数据，可以直接清空缓冲区，读写位置 归零
void Buffer::retrieveAll()                     // 检索全部
{
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}


// 将未读取的数据写入字符串，然后清空缓冲区
std::string Buffer::retrieveAllToStr()
{
    std::string str(peek(), readableBytes());   // 将可读部分写入字符串

    retrieveAll();                              // 清空缓冲区，读写位置 归零

    return str;
}


// 返回当前写位置 指针
const char* Buffer::beginWriteConst() const
{
    return beginPtr_() + writePos_;
}


// 返回当前写位置 指针
char* Buffer::beginWrite()
{
    return beginPtr_() + writePos_;
}


// 往缓冲区写入数据
void Buffer::append(const std::string& str)
{
    append(str.data(), str.length());
}


// 往缓冲区写入数据
// 所有 append 最终都调用这个 函数
void Buffer::append(const char* str, size_t len)
{
    assert(str);

    ensureWritable(len);        // 确保数据可写入

    std::copy(str, str + len, beginWrite());    // 追加写入数据

    hasWritten(len);            // 移动写指针
}


// 往缓冲区写入数据
void Buffer::append(const void* data, size_t len)
{
    assert(data);

    append(static_cast<const char*>(data), len);
}


// 从另一个缓冲区读取数据，写入到当前缓冲区
void Buffer::append(const Buffer& buff)
{
    append(buff.peek(), buff.readableBytes());
}


// 从 fd 中读取数据，写入到缓冲区中
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char buff[65535];
    struct iovec iov[2];                        // IO vector
    const size_t writable = writableBytes();    // 拿到可写入的字节数

    // 分散读，保证数据全部读完
    iov[0].iov_base = beginPtr_() + writePos_;  // 可写部分数据
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;                     // buff
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        *saveErrno = errno;
    }
    else if (static_cast<size_t>(len) <= writable) {    // 原缓冲区即可读完所有数据
        writePos_ += len;
    }
    else {                                              // 原缓冲区读不完，拓展空间 追加数据
        writePos_ = buffer_.size();
        append(buff, len - writable);
    }

    return len;
}


// 将未读取的数据 写入到 fd 中
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    size_t readSize = readableBytes();          // 拿到 可读取的字节数

    ssize_t len = write(fd, peek(), readSize);  // 将缓冲区内容 写入到 fd 中
    if (len < 0) {
        *saveErrno = errno;
        return len;
    }

    readPos_ += len;        // 更新读指针位置
    return len;
}


// 获取 缓冲区首元素地址
char* Buffer::beginPtr_()
{
    //return &*buffer_.begin();   // vector数组 首元素的地址
    return &buffer_.front();
}


// 获取 缓冲区首元素地址
const char* Buffer::beginPtr_() const
{
    //return &*buffer_.begin();   // vector数组 首元素的地址
    return &buffer_.front();
}


// 拓展空间/整理缓冲区
void Buffer::makeSpace_(size_t len)
{
    // 空间不够，拓展缓冲区大小
    if (writableBytes() + prependableBytes() < len) {   // 可写入 + 已读取空间 < len
        buffer_.resize(writePos_ + len + 1);
    }
    else {
        size_t readable = readableBytes();      // 当前可读取的数据长度

        // 将还未读取的数据往前移动，覆盖掉已经读取的数据
        std::copy(beginPtr_() + readPos_, beginPtr_() + writePos_, beginPtr_());
        
        // 更新读写位置
        readPos_ = 0;
        writePos_ = readable;

        assert(readable == readableBytes());    // 确保读写位置更新成功
    }
}