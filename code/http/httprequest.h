

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include<unordered_map>
#include<unordered_set>
#include<string>
#include<regex>
#include<errno.h>
#include<mysql/mysql.h>

#include"../buffer/buffer.h"
#include"../log/log.h"
#include"../pool/sqlconnpool.h"
#include"../pool/sqlconnRAII.hpp"


class HttpRequest {
public:
    // 解析状态
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    HttpRequest();
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer &buff);

    std::string method() const;
    std::string path() const;
    std::string& path();
    std::string version() const;
    std::string getPost(const std::string& key) const;
    std::string getPost(const char* key) const;

    bool isKeepAlive() const;

private:
    bool parseRequestLine_(const std::string& line);
    void parseHeader_(const std::string& line);
    void parseBody_(const std::string& line);

    void parsePath_();
    void parsePost_();
    void parseFromUrlEncoded_();

    static bool userVerify(const std::string& name, const std::string& pwd, bool isLogin);

    static int converHex(char ch);

    PARSE_STATE state_;                                     // 解析的状态
    std::string method_, path_, version_, body_;            // 方法、路径、版本、body
    std::unordered_map<std::string, std::string> header_;   // header 映射
    std::unordered_map<std::string, std::string> post_;     // 记录 post 请求中的 键值对(username/password)

    static const std::unordered_set<std::string> DEFAULT_HTML;          // 默认的 html 页面 哈希集合
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; // 默认的 html tag 映射
};


#endif  // HTTP_REQUEST_H
