

#include"sqlconnpool.h"
using namespace std;


SqlConnPool* SqlConnPool::instance()
{
    static SqlConnPool connPool;
    return &connPool;
}


// 从连接队列中 获取一个连接
MYSQL* SqlConnPool::getConn()
{
    MYSQL* sql = nullptr;

    // 连接队列为空
    if (connQue_.empty()) {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;                 // 返回空后 将中断程序，UserVerify函数中有断言
    }

    // 等待信号量
    sem_wait(&semId_);

    // 获取连接队列头节点
    {
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }

    return sql;
}


// 将 sql 添加回数据库池连接队列，重新等待使用需求
void SqlConnPool::freeConn(MYSQL* sql)
{
    assert(sql);

    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_);              // 增加信号量，唤醒等待的进程
}


// 返回连接队列的大小，即当前连接池中可用的连接数量
int SqlConnPool::getFreeConnCount()
{
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}


// 初始化
// void init(
//         // 主机IP，端口
//         // 用户名，密码
//         // 数据库名，连接数
//         const char* host, int port,
//         const char* user, const char* pwd,
//         const char* dbName, int connSize = 10);
void SqlConnPool::init(
    const char* host, int port,
    const char* user, const char* pwd,
    const char* dbName, int connSize)
{
    assert(connSize > 0);

    for (int i = 0; i < connSize; ++i) {
        
        // 初始化 一个 mysql 连接的实例对象
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }

        // 连接到 mysql 服务器
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql connect error!");
            assert(sql);
        }

        // 连接成功，将 sql 添加到 连接队列中
        connQue_.push(sql);
    }

    MAX_CONN_ = connSize;   // 设置最大可连接数

    // 初始化信号量
    sem_init(&semId_, 0, static_cast<unsigned int>(MAX_CONN_));
}


// 关闭 sql连接池
void SqlConnPool::closePool()
{
    lock_guard<mutex> locker(mtx_);

    // 关闭当前所有的 mysql 连接
    while (!connQue_.empty()) {
        mysql_close(connQue_.front());  // 关闭 mysql 连接
        connQue_.pop();
    }

    // 关闭 mysql 函数库调用，释放相关内存
    mysql_library_end();
}



SqlConnPool::SqlConnPool()
{
    useCount_ = 0;
    freeCount_ = 0;
}


SqlConnPool::~SqlConnPool()
{
    closePool();
}