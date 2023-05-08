

#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include<mysql/mysql.h>
#include<string>
#include<queue>
#include<mutex>
#include<semaphore.h>
#include<thread>

#include"../log/log.h"


class SqlConnPool {
public:
    static SqlConnPool* instance();

    MYSQL* getConn();
    void freeConn(MYSQL* conn);
    int getFreeConnCount();

    void init(
        // 主机IP，端口
        // 用户名，密码
        // 数据库名，连接数
        const char* host, int port,
        const char* user, const char* pwd,
        const char* dbName, int connSize = 10);

    void closePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    unsigned int MAX_CONN_;                  // 最大连接数
    
    // 这两个变量完全没用到？？
    int useCount_;                  // 当前连接数
    int freeCount_;                 // 等待释放的连接数

    std::queue<MYSQL*> connQue_;    // 连接队列
    std::mutex mtx_;                
    sem_t semId_;                   // 信号量
};


#endif  // SQLCONNPOOL_H