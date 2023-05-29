

#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include<unordered_map>
#include<fcntl.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/mman.h>

#include"../buffer/buffer.h"
#include"../log/log.h"


class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void makeResponse(Buffer& buff);
    void unmapFile();
    char* file();
    size_t fileLen() const;
    void errorContent(Buffer& buff, std::string message);
    int code() const;
    std::string& path();

private:
    void errorHtml_();
    
    void addStateLine_(Buffer &buff);
    void addHeader_(Buffer &buff);
    void addContent_(Buffer &buff);

    std::string getFileType_();

    int code_;                  // 要返回的http状态码
    bool isKeepAlive_;          // 是否保持连接

    std::string path_;          // 具体资源所在路径
    std::string srcDir_;        // 工作目录

    char* mmFile_;              // 内存区的映射地址
    struct stat mmFileStat_;    // 文件属性结构体

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 后缀类型To路径
    static const std::unordered_map<int, std::string> CODE_STATUS;          // 状态码To原因
    static const std::unordered_map<int, std::string> CODE_PATH;            // 状态码Tohtml页面路径
};


#endif  // HTTP_RESPONSE_H
