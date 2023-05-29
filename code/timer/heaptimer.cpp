

#include"heaptimer.h"


// 调整指定 id 的节点，设置新的到期时间
void HeapTimer::adjust(int id, int newExpires)
{
    assert(!heap_.empty() && ref_.count(id));
    
    // 更新到期时间
    heap_[ref_[id]].expires = Clock::now() + MS(newExpires);

    // 将节点放到最后
    siftdown_(ref_[id], heap_.size());
}


// 添加新的定时器节点
void HeapTimer::add(int id, int timeOut, const timeoutCallBack& cb)
{
    assert(0 <= id);

    size_t index = 0;
    if (ref_.count(id) == 0) {      // 新节点
        index = heap_.size();                                       // 节点插入到堆尾
        ref_[id] = index;                                           // 更新映射表
        heap_.push_back({ id, Clock::now() + MS(timeOut), cb});     // 插入堆尾
        siftup_(index);                                             // 将节点更新到合适位置
    }
    else {                          // 已有节点
        index = ref_[id];
        heap_[index].expires = Clock::now() + MS(timeOut);          // 更新到期时间
        heap_[index].cb = cb;                                       // 更新回调函数
        if (!siftdown_(index, heap_.size())) {      // 将节点更新到合适位置，一般 到期时间会增加
            siftup_(index);                         // 节点位置没改变，可能到期时间减少了，往上更新位置
        }
    }
}


// 删除指定 id 节点，并触发回调函数
void HeapTimer::doWork(int id)
{
    // id 节点不存在
    if (heap_.empty() || ref_.count(id) == 0) {
        return;
    }

    // 调用 回调函数
    size_t index = ref_[id];
    // timerNode node = heap_[index];
    // node.cb();
    heap_[index].cb();

    // 删除该节点
    del_(index);
}


// 清空 定时器节点堆、映射表
void HeapTimer::clear()
{
    heap_.clear();
    ref_.clear();
}


// 时钟滴答，清除超时节点
void HeapTimer::tick()
{
    // 没有节点
    if (heap_.empty()) {
        return;
    }

    while (!heap_.empty()) {

        timerNode node = heap_.front();     // 拿到头节点

        // 未到 到期时间
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }

        node.cb();  // 调用 回调函数

        pop();      // 弹出头节点
    }
}


// 弹出头节点
void HeapTimer::pop()
{
    assert(!heap_.empty());

    del_(0);    // 删除头结点
}


// 返回最近一次 时钟到期所剩时间
int HeapTimer::getNextTick()
{
    // 先执行一次 时钟滴答，清除已经到期的定时器
    tick();

    int64_t res = 0;
    if (!heap_.empty()) {
        // 获取头节点的到期时间
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0) {      // 在执行的瞬间，头节点到期了
            res = 0;
        }
    }

    return static_cast<int>(res);
}


// 删除指定位置的节点
void HeapTimer::del_(size_t index)
{
    assert(!heap_.empty() && 0 <= index && index < heap_.size());

    size_t n = heap_.size() - 1;

    // 如果不是 尾节点
    if (index < n) {
        swapNode_(index, n);            // 将 要删除的节点 换到队尾
        if (!siftdown_(index, n)) {     // 更新 原队尾节点 的位置
            siftup_(index);
        }
    }

    // 删除队尾元素
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}


// 将 index 节点往上更新到合适的位置
void HeapTimer::siftup_(size_t index)
{
    assert(0 <= index && index < heap_.size());

    size_t childIndex = index;                         // 当前子节点
    size_t parentIndex = (childIndex - 1) / 2;         // 父节点

    while (parentIndex >= 0) {
        if (heap_[parentIndex] < heap_[childIndex]) {  // 父节点 比 当前子节点 小
            break;
        }

        swapNode_(childIndex, parentIndex);            // 将更小的子节点换上来，原父节点换下去
        childIndex = parentIndex;                      // 重复以上步骤，将 当前子节点 往上 换到合适的位置
        parentIndex = (childIndex - 1) / 2;
    }
}


// 将 index 节点往下更新到合适的位置，n 为heap应该有的大小（可能是删除节点后的堆大小）
bool HeapTimer::siftdown_(size_t index, size_t n)
{
    assert(0 <= index && index < heap_.size());
    assert(0 <= n && n <= heap_.size());

    size_t parentIndex = index;       // 父节点节点
    size_t childIndex = parentIndex * 2 + 1;   // 左子节点

    while (childIndex < n) {
        if (childIndex + 1 < n && heap_[childIndex + 1] < heap_[childIndex]) {  // 右子节点 比 左子节点 小
            ++childIndex;                                                       // childIndex 更新为 右子节点索引
        }
        if (heap_[parentIndex] < heap_[childIndex]) {                           // 父节点 比 两个子节点 都小
            break;                                                              // 跳出循环
        }

        swapNode_(parentIndex, childIndex);                                     // 将更小的子节点换上来，原父节点换下去
        parentIndex = childIndex;                                               // 重复以上步骤，直到原父节点换到合适的位置
        childIndex = parentIndex * 2 + 1;
    }

    return parentIndex > index;   // 只要原父节点位置变了，就返回真
}


// 交换节点
void HeapTimer::swapNode_(size_t iIndex, size_t jIndex)
{
    assert(0 <= iIndex && iIndex < heap_.size());
    assert(0 <= jIndex && jIndex < heap_.size());

    ref_[heap_[iIndex].id] = jIndex;
    ref_[heap_[jIndex].id] = iIndex;

    std::swap(heap_[iIndex], heap_[jIndex]);
}
