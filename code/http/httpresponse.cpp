

#include"httpresponse.h"
using namespace std;


// 静态成员变量 类外定义
// 后缀类型 To 资源类型，用于填充 响应头中的 Content-type 字段
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

// 状态码 To 原因
const unordered_map<int, string> HttpResponse::CODE_STATUS {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

// 状态码 To html页面路径
const unordered_map<int, string> HttpResponse::CODE_PATH {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};



HttpResponse::HttpResponse()
{
    code_ = -1;
    isKeepAlive_ = false;

    //path_ = srcDir_ = "";

    mmFile_ = nullptr;
    mmFileStat_ = { 0 };
}


HttpResponse::~HttpResponse()
{
    unmapFile();
}


// 初始化
//void init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
void HttpResponse::init(const string& srcDir, string& path, bool isKeepAlive, int code)
{
    assert(srcDir != "");

    // 如果 mmFile_ 不为空，先解除映射
    if (mmFile_) {
        unmapFile();
    }

    code_ = code;
    isKeepAlive_ = isKeepAlive;

    path_ = path;
    srcDir_ = srcDir;

    mmFile_ = nullptr;
    mmFileStat_ = { 0 };
}


// 做出响应，响应信息保存在 buff
void HttpResponse::makeResponse(Buffer& buff)
{
    // 判断请求的资源文件
    // 文件不存在 或所请求资源为目录
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    }
    else if (!(mmFileStat_.st_mode & S_IROTH)) {    // 文件权限为 other不可读，即资源不可获取
        code_ = 403;
    }
    else if (code_ == -1) {                         // code_ 未设置，则成功获取资源
        code_ = 200;
    }

    // 处理400系列的页面
    errorHtml_();

    // 正常页面处理
    addStateLine_(buff);
    addHeader_(buff);
    addContent_(buff);
}


// 解除 文件映射
void HttpResponse::unmapFile()
{
    if (mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}


// 返回 共享内存区的映射地址 mmFile_
char* HttpResponse::file()
{
    return mmFile_;
}


// 返回 共享内存区的大小
size_t HttpResponse::fileLen() const
{
    return mmFileStat_.st_size;
}


// 根据message、生成对应的html错误页面、写入到buff中
void HttpResponse::errorContent(Buffer& buff, string message)
{
    string body;    // 生成的html错误页面
    string status;  // 状态原因信息

    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";

    if (CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        status = "Bad Request";                     // 非法请求
    }

    body += to_string(code_) + " : " + status + "\n";       // 状态码 : 状态原因信息
    body += "<p>" + message + "</p>";                       // 错误内容
    body += "<hr><em>TinyWebServer</em></body></html>";

    // 将生成的错误信息 写入到缓冲区
    buff.append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}


// 返回 状态码code_
int HttpResponse::code() const
{
    return code_;
}


// 返回 path_
string& HttpResponse::path()
{
    return path_;
}


// 设置400系列具体页面路径、获取资源文件属性
void HttpResponse::errorHtml_()
{
    // 状态码为400系列，error
    if (CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;          // 设置对应的 具体页面路径
        stat((srcDir_ + path_).data(), &mmFileStat_);   // 获取资源文件的 文件属性信息
    }
}


// 添加 响应状态行
void HttpResponse::addStateLine_(Buffer &buff)
{
    string status;      // 状态原因信息

    if (CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {                                      // 没有状态 就是400非法请求
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }

    // 生成 状态行，添加到缓冲区
    buff.append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}


// 添加 响应头部
void HttpResponse::addHeader_(Buffer &buff)
{
    buff.append("Connection: ");

    if (isKeepAlive_) {             // 仍保持连接
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");  // 最大连接数6，超时时间120s
    }
    else {                          // 连接已断开
        buff.append("close\r\n");
    }

    buff.append("Content-type: " + getFileType_() + "\r\n");    // 响应的资源类型
}


// 添加 响应正文
void HttpResponse::addContent_(Buffer &buff)
{
    // 只读方式 打开资源
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0) {
        errorContent(buff, "File NotFound!");   // 找不到资源文件
        return;
    }

    LOG_DEBUG("Response file path: %s", (srcDir_ + path_).data());

    // 将文件映射到内存 提高文件的访问速度
    // MAP_PRIVATE 建立一个写入时拷贝的私有映射，速度更快
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1) {
        errorContent(buff, "File NotFound!");   // 内存区映射失败
        return;
    }

    // 设置 内存区的映射地址
    mmFile_ = (char*)mmRet;
    
    close(srcFd);
    
    // 将文件大小信息 添加到缓冲区
    buff.append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}


// 获取文件类型，返回对应的资源类型
string HttpResponse::getFileType_()
{
    // 判断文件类型
    string::size_type idx = path_.find_last_of('.');
    if (idx == string::npos) {      // 没有找到 文件类型后缀
        return "text/plain";        // 返回空白页面
    }

    string suffix = path_.substr(idx);              // 获取文件后缀
    if (SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;    // 返回对应的资源类型
    }

    return "text/plain";
}