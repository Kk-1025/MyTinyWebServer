

#include"log.h"
using namespace std;



Log::Log()
{
    lineCount_ = 0;

    fp_ = nullptr;

    deque_ = nullptr;
    writeThread_ = nullptr;
}


Log::~Log()
{
    // 开启异步/有写线程 且写线程可被回收
    if (writeThread_ && writeThread_->joinable()) {

        while (!deque_->empty()) {      // 弹出异步阻塞队列所有节点，写入所有日志
            deque_->flush();
        }

        deque_->Close();                // 关闭阻塞队列
        writeThread_->join();           // 回收写线程
    }

    if (fp_) {
        lock_guard<mutex> locker(mtx_);

        flush();                        // 刷新缓冲区、马上写入所有日志数据到文件
        fclose(fp_);                    // 关闭文件
    }
}


// 初始化工作
// void init(
//         // 日志等级，日志存放路径
//         // 日志文件后缀，异步队列容量（输入有效容量才开启异步，否则同步）
//         int level = 1, const char* path = "./log", 
//         const char* suffix = ".log", int maxQueueCapacity = 1024);
void Log::init(
    int level, const char* path, 
    const char* suffix, int maxQueueCapacity)
{
    isOpen_ = true;
    level_ = level;

    // 有效的最大容量
    if (maxQueueCapacity > 0) {
        isAsync_ = true;            // 异步

        if (!deque_) {      // 阻塞队列未创建
            // 初始化 阻塞队列
            unique_ptr<BlockDeque<string>> newDeque(new BlockDeque<string>);
            deque_ = move(newDeque);

            // 初始化 写线程
            unique_ptr<thread> newThread(new thread(flushLogThread));
            writeThread_ = move(newThread);
        }
    }
    else {      // 最大队列容量不大于0
        isAsync_ = false;           // 同步
    }

    //lineCount_ = 0;

    time_t timer = time(nullptr);               // 返回当前的时间
    struct tm* sysTime = localtime(&timer);     // 将 time_t 转换成 struct tm，获取更多的时间信息
    struct tm t = *sysTime;
    
    path_ = path;
    suffix_ = suffix;
    toDay_ = t.tm_mday;

    // 文件名
    char fileName[LOG_NAME_LEN] = { 0 };

    // 生成日志文件的路径
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);

    // 创建日志文件
    {
        lock_guard<mutex> locker(mtx_);

        // 先清空缓冲区，以备后用
        buff_.retrieveAll();

        // 如果已经有指向的文件结构体，先关闭
        if (fp_) {
            flush();            // 刷新缓冲区、马上写入日志
            fclose(fp_);
        }

        // 以 append 追加方式 打开日志文件，文件不存在即自动创建
        fp_ = fopen(fileName, "a");
        if (fp_ == nullptr) {           // 打开文件失败，可能是给定路径不是有效的目录
            mkdir(path_, 0777);         // 先创建对应的目录
            fp_ = fopen(fileName, "a"); // 再尝试打开文件
        }

        assert(fp_ != nullptr);
    }
}


// 单例模式：局部静态变量的懒汉模式
Log* Log::instance()
{
    static Log inst;
    return &inst;
}


// 写线程的工作函数，即不断从异步阻塞队列中取出节点数据、完成日志的写入
void Log::flushLogThread()
{
    // 完成一次异步写，将阻塞队列中的所有内容 写入到日志文件中
    Log::instance()->AsyncWrite_();
}


// 写日志，level：日志等级、类型，format：附加参数的数量
void Log::write(int level, const char* format, ...)
{
    // 获取当前的精确时间，微秒级
    struct timeval now = { 0, 0 };
    gettimeofday(&now, nullptr);
    
    // 拿到秒数，转换成 更多的时间信息
    time_t tSec = now.tv_sec;
    struct tm* sysTime = localtime(&tSec);
    struct tm t = *sysTime;

    // 可变参数列表：variable arguments list，保存可变参数的信息
    va_list vaList;

    // 如果 日期发生了变化、或 刚好写够 MAX_LINES行 日志，生成新的日志文件
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        // 先解锁
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = { 0 };          // 日期信息
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        // 生成日志文件路径，路径+日期信息+日志数+后缀
        if (toDay_ != t.tm_mday) {      // 日期发生变化的情况
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;         // 更新日期
            lineCount_ = 0;
        }
        else {                          // 写满日志的情况
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_ / MAX_LINES), suffix_);
        }

        // 加锁
        locker.lock();

        flush();                    // 刷新缓冲区、马上写入日志到文件
        fclose(fp_);                // 关闭旧日志文件
        fp_ = fopen(newFile, "a");  // 打开新日志文件

        assert(fp_ != nullptr);
    }

    // 正常的写日志
    {
        unique_lock<mutex> locker(mtx_);

        // 增加写日志行数
        ++lineCount_;

        // 写入：年-月-日 hour:min:sec.usec
        int n = snprintf(buff_.beginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                        t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        buff_.hasWritten(n);        // 移动写缓冲区指针

        // 缓冲区添加日志类型标头
        appendLogLevelTitle_(level);

        // 初始化 va_list，传入附加参数的数量
        va_start(vaList, format);
        
        // 将格式化数据 写入到缓冲区
        int m = vsnprintf(buff_.beginWrite(), buff_.writableBytes(), format, vaList);
        
        // 停止使用 va_list，销毁 va_list
        va_end(vaList);

        buff_.hasWritten(m);        // 移动写缓冲区指针
        buff_.append("\n\0", 2);    // 输出一个换行符

        // 异步 且队列未满
        if (isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.retrieveAllToStr());    // 先将缓冲区数据添加进异步阻塞队列，再等待flush写入
        }
        else {      // 同步 或异步阻塞队列已满
            fputs(buff_.peek(), fp_);                       // 马上将缓冲区的数据写出到日志文件中
        }

        // 完成一次写日志，清空缓冲区
        buff_.retrieveAll();
    }
}


// 如果异步、弹出异步阻塞队列所有节点，刷新系统缓冲区
void Log::flush()
{
    // 如果开启异步，弹出阻塞队列所有节点
    if (isAsync_) {
        deque_->flush();
    }

    // 马上刷新一次系统缓冲区，写入所有未写入的数据
    fflush(fp_);
}


// 返回 日志等级/类型
int Log::getLevel()
{
    lock_guard<mutex> locker(mtx_);

    return level_;
}


// 设置 日志等级/类型
void Log::setLevel(int level)
{
    lock_guard<mutex> locker(mtx_);

    level_ = level;
}


// 添加日志等级标题
void Log::appendLogLevelTitle_(int level)
{
    switch (level)
    {
    case 0:
        buff_.append("[debug]: ", 9);
        break;
    case 1:
        buff_.append("[info]:  ", 9);
        break;
    case 2:
        buff_.append("[warn]:  ", 9);
        break;
    case 3:
        buff_.append("[error]: ", 9);
        break;
    default:
        buff_.append("[info]:  ", 9);
        break;
    }
}


// 异步写，写线程工作使用的函数；拿到异步阻塞队列节点的数据 输出到日志文件中
void Log::AsyncWrite_()
{
    string str;
    // 只要异步阻塞队列没有关闭，就弹出队列的节点；弹出需要等到消费者条件变量
    while (deque_->pop(str)) {              // 只有这一个地方调用了pop，因此队列为空阻塞时、只要调用flush就可继续写

        lock_guard<mutex> locker(mtx_);
    
        fputs(str.c_str(), fp_);            // 将弹出的所有节点的数据 写入到日志文件
    }
}
