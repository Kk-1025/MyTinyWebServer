

#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include<queue>
#include<unordered_map>
#include<time.h>
#include<algorithm>
#include<arpa/inet.h>
#include<functional>
#include<assert.h>
#include<chrono>        // 时间编程库

#include"../log/log.h"


using timeoutCallBack = std::function<void()>;      // 形如 void () 的 function类型
using Clock = std::chrono::high_resolution_clock;   // 时钟         Clocks      高分辨率的时钟
using MS = std::chrono::milliseconds;               // 时间间隔      Duration   毫秒
using timeStamp = Clock::time_point;                // 时间戳/点     Time point


// 计时器节点 结构体
struct timerNode {
    int id;
    timeStamp expires;      // 有效期/到期时间
    timeoutCallBack cb;     // 到期时调用的 回调函数

    bool operator<(const timerNode& t) {        // 重载小于运算符
        return expires < t.expires;
    }
};


class HeapTimer {
public:
    HeapTimer() {
        heap_.reserve(64);      // 只拓展 capacity，不影响 size
    }
    ~HeapTimer() {
        clear();
    }

    void adjust(int id, int newExpires);
    void add(int id, int timeOut, const timeoutCallBack& cb);
    void doWork(int id);

    void clear();
    void tick();
    void pop();
    int getNextTick();

private:
    void del_(size_t i);
    void siftup_(size_t i);                     // 筛选
    bool siftdown_(size_t index, size_t n);
    void swapNode_(size_t i, size_t j);

    std::vector<timerNode> heap_;           // 计时器节点 数组，最小堆
    std::unordered_map<int, size_t> ref_;   // 映射，节点 id To heap_Index
};


#endif  // HEAP_TIMER_H