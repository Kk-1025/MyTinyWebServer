

#ifndef LOG_H
#define LOG_H

#include<mutex>
#include<string>
#include<thread>
#include<sys/time.h>
#include<string.h>
#include<stdarg.h>
#include<assert.h>
#include<sys/stat.h>

#include"blockqueue.hpp"
#include"../buffer/buffer.h"


class Log {
public:
    void init(
        // 日志等级，日志存放路径
        // 日志文件后缀，异步队列容量（输入有效容量才开启异步，否则同步）
        int level = 1, const char* path = "./log", 
        const char* suffix = ".log", int maxQueueCapacity = 1024);
    
    static Log* instance();
    static void flushLogThread();

    void write(int level, const char* format, ...);
    void flush();

    int getLevel();
    void setLevel(int level);
    bool isOpen() { return isOpen_; }

private:
    Log();
    virtual ~Log();
    void appendLogLevelTitle_(int level);
    void AsyncWrite_();         // 异步写


    static const int LOG_PATH_LEN = 256;    // 日志文件路径最大长度
    static const int LOG_NAME_LEN = 256;    // 日志文件名的最大长度
    static const int MAX_LINES = 50000;     // 单个日志文件的最大写入行数

    const char* path_;      // 日志文件路径
    const char* suffix_;    // 文件后缀名
    
    int lineCount_;     // 写入日志总行数
    int toDay_;         // 当前日期（day）

    bool isOpen_;       // 文件是否打开
    
    Buffer buff_;       // 缓冲区
    int level_;         // 日志等级
    bool isAsync_;      // 是否开启异步

    FILE* fp_;          // 日志文件结构体指针
    std::unique_ptr<BlockDeque<std::string>> deque_;    // 异步阻塞队列，开启异步才使用
    std::unique_ptr<std::thread> writeThread_;          // 写线程，开启异步才使用
    std::mutex mtx_;
};


// 完成一次写日志操作，即生成一行日志信息、并马上写入到文件
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::instance();\
        if (log->isOpen() && log->getLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__);\
            log->flush();\
        }\
    } while(0);

// 默认日志等级为 1，即 DEBUG 日志不会记录
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif  // LOG_H
