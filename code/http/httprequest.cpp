

#include"httprequest.h"
using namespace std;


// 静态成员变量 类外初始化

// 默认的 html 页面 哈希集合
const unordered_set<string> HttpRequest::DEFAULT_HTML {
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};
// html tag 映射，注册/登录页面 To Int
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register.html", 0},  // 注册
    {"/login.html", 1},     // 登录
};



HttpRequest::HttpRequest()
{
    init();
}


// 初始化
void HttpRequest::init()
{
    state_ = REQUEST_LINE;
}


// 从 buff 中获取请求信息，解析请求
bool HttpRequest::parse(Buffer &buff)
{
    const char CRLF[] = "\r\n";     // 回车换行

    // 缓冲区无可读
    if (buff.readableBytes() <= 0) {
        return false;
    }

    // 只要 缓冲区可读、state_ 不为 FINISH
    while (buff.readableBytes() && state_ != FINISH) {

        // 在缓冲区中 找回车换行，一次只读一行数据
        const char* lineEnd = search(buff.peek(), buff.beginWriteConst(), CRLF, CRLF + 2);
        string line(buff.peek(), lineEnd);

        // 状态机：根据当前请求状态 来处理对应信息
        switch (state_)
        {
        case REQUEST_LINE:      // 请求行
            if (!parseRequestLine_(line)) {     // 解析请求行
                return false;
            }
            parsePath_();                       // 解析路径
            break;
        case HEADERS:           // 请求头
            parseHeader_(line);                 // 解析 headers
            if (buff.readableBytes() <= 2) {    // 可读字节不够，意味没有 请求正文 可读
                state_ = FINISH;
            }
            break;
        case BODY:              // 请求正文（GET请求没有）
            parseBody_(line);                   // 解析 body
            break;
        default:                // FINISH，请求信息已读取、解析完成
            break;
        }

        // 读到最后一行数据，跳出循环
        if (lineEnd == buff.beginWrite()) {
            break;
        }

        // 移动缓冲区读指针
        buff.retrieveUntil(lineEnd + 2);
    }

    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());

    return true;
}


// 返回 method_
string HttpRequest::method() const
{
    return method_;
}


// 返回 path_
string HttpRequest::path() const
{
    return path_;
}

string& HttpRequest::path()
{
    return path_;
}


// 返回 version_
string HttpRequest::version() const
{
    return version_;
}


// 返回 post_表中 key映射的val
string HttpRequest::getPost(const string& key) const
{
    assert(key != "");

    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }

    return "";
}

string HttpRequest::getPost(const char* key) const
{
    assert(key != nullptr);

    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }

    return "";
}


// 判断是否保持连接
bool HttpRequest::isKeepAlive() const
{
    // header_中Connection的值为keep-alive 且 version_为1.1，才为有效连接
    if (header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }

    return false;
}


// 解析 请求行
bool HttpRequest::parseRequestLine_(const string& line)
{
    // 正则表达式
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");     // 请求方法 URI HTTP协议版本
    smatch subMatch;

    // 正则表达式匹配上，获取请求行信息
    if (regex_match(line, subMatch, patten)) {
        method_ = subMatch[1];          // 请求方法
        path_ = subMatch[2];            // URI
        version_ = subMatch[3];         // HTTP协议版本

        state_ = HEADERS;               // 将解析状态 更新为 解析headers

        return true;
    }

    LOG_ERROR("Parse RequestLine Error");

    return false;
}


// 解析 请求头
void HttpRequest::parseHeader_(const string& line)
{
    // 正则表达式，key: val
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;

    // 正则表达式匹配上，加入 header_哈希表
    if (regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {      // 匹配不上、即匹配完所有请求头信息，则将解析状态 更新为 解析请求正文
        state_ = BODY;
    }
}


// 解析 请求正文，只限于 POST 请求
void HttpRequest::parseBody_(const string& line)
{
    body_ = line;

    // 解析 post 请求（如果为post才真正执行）
    parsePost_();

    // 更新解析状态 为解析完成
    state_ = FINISH;

    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}


// 解析 请求资源的路径
void HttpRequest::parsePath_()
{
    if (path_ == "/") {     // 默认主页面
        path_ = "/index.html";
    }
    else {                  // 从已有默认html页面中选取
        for (auto &item : DEFAULT_HTML) {
            if (item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}


// 解析 post 请求
void HttpRequest::parsePost_()
{
    // 为POST请求，且该 url 被编码过
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        
        // 将 url 解码
        parseFromUrlEncoded_();

        // 请求合理
        if (DEFAULT_HTML_TAG.count(path_)) {
            
            // 拿到操作对应的 tag
            int tag = DEFAULT_HTML_TAG.find(path_)->second;

            // 打印日志
            LOG_DEBUG("%s, Tag:%d", path_.c_str(), tag);

            // 请求为 注册或登录
            if (tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);  // 是否为登录操作

                // 检验用户身份
                if (userVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";    // 注册/登录成功
                }
                else {
                    path_ = "/error.html";      // 注册/登录失败
                }
            }
        }
    }
}


// 将 url 解码
void HttpRequest::parseFromUrlEncoded_()
{
    // 请求正文为空
    if (body_.size() == 0) {
        return;
    }

    string key, value;
    int num = 0;
    int n = body_.size();   // 请求正文的长度
    int i = 0, j = 0;

    post_.clear();

    // 遍历请求正文的每个字符
    for (; i < n; ++i) {
        char ch = body_[i];
        
        switch (ch)
        {
        case '=':   // key=value 键值对
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
            
        case '+':   // 空格符
            body_[i] = ' ';
            break;

        case '%':   // 十六进制 转 十进制
            num = converHex(body_[i + 1] * 16 + converHex(body_[i + 2]));
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;

        case '&':   // 结束一对键值对
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;

            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;

        default:
            break;
        }
    }

    assert(j <= i);

    // 补上最后一对 键值对
    if (post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
        
        LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
    }
}



// 静态函数

// 检验用户身份
bool HttpRequest::userVerify(const string& name, const string& pwd, bool isLogin)
{
    // 错误输入
    if (name == "" || pwd == "") {
        return false;
    }

    LOG_INFO("UserVerify name:%s pwd:%s", name.c_str(), pwd.c_str());

    // 从 mysql池中 获取一个 mysql 连接对象
    MYSQL* sql = nullptr;
    // RAII对象：创建对象时 获取mysql连接对象；销毁对象时，自动销毁mysql连接对象
    SqlConnRAII(&sql, SqlConnPool::instance());
    assert(sql);

    bool flag = false;          // true为正确 注册/登录操作，false为错误 注册/登录操作

    char order[256] = { 0 };    // 记录 sql 命令
    
    // 注册操作
    if (!isLogin) {
        flag = true;
    }

    // 生成 查询用户、密码 的命令
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    
    LOG_DEBUG("%s", order);

    // 执行 查询命令
    if (mysql_query(sql, order)) {          // 执行失败
        
        LOG_DEBUG("%s", mysql_error(sql));  // 生成错误原因
        
        return false;
    }

    // 存储 查询结果
    MYSQL_RES* res = mysql_store_result(sql);

    // 成功查询到 该用户名 的信息
    while (MYSQL_ROW row = mysql_fetch_row(res)) {      // 返回结果集中下一行的结构
        // 打印日志
        LOG_DEBUG("MYSQL ROW: %s | %s", row[0], row[1]);  // username, password

        string password(row[1]);

        if (isLogin) {                  // 登录操作

            if (pwd == password) {          // 成功登录
                flag = true;
            }
            else {                          // 登录失败，密码错误
                flag = false;
                LOG_DEBUG("pwd error!");    // 打印日志
            }
        }
        else {                          // 注册操作，数据库已有该用户名、注册失败
            flag = false;
            LOG_DEBUG("user used!");
        }
    }

    // 释放结果集
    mysql_free_result(res);

    // 未查询到 该用户名 信息、注册行为、且用户名未被使用
    if (!isLogin && flag == true) {

        LOG_DEBUG("regirster!");
        
        // 生成 插入新用户数据 的命令
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", name.c_str(), pwd.c_str());

        LOG_DEBUG("%s", order);

        // 执行 插入命令
        if (mysql_query(sql, order)) {                          // 执行失败

            LOG_DEBUG("Insert error: %s", mysql_error(sql));    // 生成错误原因
            flag = false;
        }

        // 执行成功
        flag = true;
    }

    // 关闭连接
    SqlConnPool::instance()->freeConn(sql);

    LOG_DEBUG("UserVerify done!");       // 完成 UserVerify 操作

    return flag;
}


// 十六进制 转 十进制
int HttpRequest::converHex(char ch)
{
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return ch;
}
