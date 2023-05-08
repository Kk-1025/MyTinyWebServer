


#include<unistd.h>
#include"server/webserver.h"


int main() {

    WebServer server(
        12345, 3, 60000, false,                  // 监听端口，ET模式，timeoutMs，优雅退出
        3306, "root", "Kjr22165.", "yourdb",     // Mysql：端口，用户名，密码，数据库名
        12, 6, true, 1, 1024                     // 连接池数量，线程池数量，日志开关、等级、异步队列容量
    );

    server.start();
}