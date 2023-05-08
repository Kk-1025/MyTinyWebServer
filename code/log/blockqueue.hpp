

#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include<mutex>
#include<deque>
#include<condition_variable>
#include<sys/time.h>


template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t maxCapacity = 1000);
    ~BlockDeque();

    void clear();
    bool empty();
    bool full();
    void Close();
    size_t size();
    size_t capacity();

    T front();
    T back();

    void push_back(const T &item);
    void push_front(const T &item);

    bool pop(T &item);
    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;                     // 阻塞队列

    size_t capacity_;                       // 容量

    std::mutex mtx_;

    bool isClose_;

    std::condition_variable condConsumer_;  // 消费者 条件变量
    std::condition_variable condProducer_;  // 生产者 条件变量
};



//explicit BlockDeque(size_t maxCapacity = 1000);
template<class T>
BlockDeque<T>::BlockDeque(size_t maxCapacity)
    :capacity_(maxCapacity)
{
    assert(maxCapacity > 0);

    isClose_ = false;
}


template<class T>
BlockDeque<T>::~BlockDeque()
{
    Close();
}


// 清空阻塞队列
template<class T>
void BlockDeque<T>::clear()
{
    std::lock_guard<std::mutex> locker(mtx_);

    deq_.clear();
}


// 返回 队列是否为空
template<class T>
bool BlockDeque<T>::empty()
{
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.empty();
}


// 返回 队列是否已满
template<class T>
bool BlockDeque<T>::full()
{
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.size() >= capacity_;
}


// 关闭阻塞队列
template<class T>
void BlockDeque<T>::Close()
{
    {
        std::lock_guard<std::mutex> locker(mtx_);

        deq_.clear();
        isClose_ = true;
    }

    // 通知所有生产者、消费者 关闭
    condProducer_.notify_all();
    condConsumer_.notify_all();
}


// 返回 队列大小
template<class T>
size_t BlockDeque<T>::size()
{
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.size();
}


// 返回 队列最大容量
template<class T>
size_t BlockDeque<T>::capacity()
{
    std::lock_guard<std::mutex> locker(mtx_);

    return capacity_;
}


// 返回 首元素
template<class T>
T BlockDeque<T>::front()
{
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.front();
}


// 返回 尾元素
template<class T>
T BlockDeque<T>::back()
{
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.back();
}


// 尾插元素
template<class T>
void BlockDeque<T>::push_back(const T &item)
{
    std::unique_lock<std::mutex> locker(mtx_);

    // 队列已满
    while (deq_.size() >= capacity_) {
        condProducer_.wait(locker);     // 先解锁，生产者等待，再加锁
    }

    deq_.push_back(item);

    condConsumer_.notify_one();         // 通知一个消费者工作
}


// 头插元素
template<class T>
void BlockDeque<T>::push_front(const T &item)
{
    std::unique_lock<std::mutex> locker(mtx_);

    // 队列已满，生产者等待锁
    while (deq_.size() >= capacity_) {
        condProducer_.wait(locker);     // 先解锁，生产者等待，再加锁
    }

    deq_.push_front(item);

    condConsumer_.notify_one();         // 通知一个消费者工作
}


// 弹出元素，item为传入传出参数；队列为空时 等待flush
template<class T>
bool BlockDeque<T>::pop(T &item)
{
    std::unique_lock<std::mutex> locker(mtx_);

    while (deq_.empty()) {          // 队列为空
        condConsumer_.wait(locker); // 先解锁，消费者等待，再加锁

        if (isClose_) {             // 要关闭 阻塞队列
            return false;
        }
    }

    // 队列不为空
    item = deq_.front();            // 弹出队头元素
    deq_.pop_front();

    condProducer_.notify_one();     // 通知一个生产者

    return true;
}


// timeout时间内 弹出元素，超时返回
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout)
{
    std::unique_lock<std::mutex> locker(mtx_);

    while (deq_.empty()) {          // 队列为空
        
        // 先解锁，消费者等待，再解锁；timeout 时间等不到，则返回
        // 超时、等不到的情况
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        }

        if (isClose_) {             // 要关闭 阻塞队列
            return false;
        }
    }

    item = deq_.front();            // 弹出队头元素
    deq_.pop_front();

    condProducer_.notify_one();     // 通知一个生产者

    return true;
}


// 刷新，通知一个消费者、即弹出一个节点
template<class T>
void BlockDeque<T>::flush()
{
    condConsumer_.notify_one();     // 通知一个消费者
}


#endif  // BLOCKQUEUE_H