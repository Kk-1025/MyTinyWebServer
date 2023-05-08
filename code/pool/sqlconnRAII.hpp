

#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H

#include"sqlconnpool.h"


// 资源 在对象构造时 初始化，在对象析构时 释放
class SqlConnRAII {
public:
    // 在构造函数中申请内存资源，即从连接池中获取一个 mysql 连接对象；sql 为传入传出参数
    SqlConnRAII(MYSQL** sql, SqlConnPool* connPool) {

        assert(connPool);

        *sql = connPool->getConn();
        sql_ = *sql;
        connPool_ = connPool;
    }

    // 在析构函数中释放内存资源，即使用完 mysql 连接对象后、将其放回连接池中
    ~SqlConnRAII() {
        if (sql_) {
            connPool_->freeConn(sql_);
        }
    }


private:
    MYSQL* sql_;
    SqlConnPool* connPool_;
};


#endif  // SQLCONNRAII_H