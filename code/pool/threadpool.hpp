

#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include<mutex>
#include<condition_variable>
#include<queue>
#include<thread>
#include<functional>
#include<assert.h>


class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8)
        : pool_(std::make_shared<Pool>()) {
        
        assert(threadCount > 0);
        
        // 循环创建工作线程
        for (size_t i = 0; i < threadCount; ++i) {

            // 捕获列表只能显式捕获lambda所在函数的局部变量，或隐式捕获对象的this指针
            std::shared_ptr<Pool> pool(pool_);      // 创建一个局部变量给lambda使用

            //std::thread([pool = pool_] {
            std::thread([pool] {                    // ??只能用值传递

                std::unique_lock<std::mutex> locker(pool->mtx);         // unique_lock 可随时解锁加锁

                // 每个线程的工作流程
                while (true) {
                    if (!pool->tasks.empty()) {     // 任务队列不为空，完成任务
                        auto task = std::move(pool->tasks.front());     // 拿到第一个任务
                        pool->tasks.pop();
                        locker.unlock();    // 取出任务后就 解锁

                        task();             // 完成任务
                        
                        locker.lock();      // 完成任务后 继续加锁
                    }
                    else if (pool->isClosed) {      // 线程池关闭，退出线程工作
                        break;
                    }
                    else {
                        pool->cond.wait(locker);    // 没有任务，等待任务，先解锁、有任务来了再加锁 处理
                    }
                }
            }).detach();    // 线程创建即分离，交给操作系统管理
        }
    }

    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool() {
        if (static_cast<bool>(pool_)) {     // 如果线程池指针不为空

            // 使用"{ }"限定lock_guard作用域，它在自身作用域（生命周期）中具有构造时加锁，析构时解锁的功能
            {                                                          
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }

            pool_->cond.notify_all();       // 通知所有线程退出
        }
    }

    template<class T>
    void addTask(T&& task) {

        {
            std::lock_guard<std::mutex> locker(pool_->mtx);     // 线程池加锁
            pool_->tasks.emplace(std::forward<T>(task));        // 加入一个任务到任务队列中
            // forward 完美转发，保持原来的值属性不变；减少内存拷贝
        }

        pool_->cond.notify_one();   // 通知一个线程工作
    }

private:
    struct Pool {                       // 线程池结构体
        std::mutex mtx;                             // 锁
        std::condition_variable cond;               // 条件变量
        bool isClosed;                              // 是否关闭
        std::queue<std::function<void()>> tasks;    // 任务队列
    };

    std::shared_ptr<Pool> pool_;        // 线程池指针
};


#endif  // THREADPOOL_HPP