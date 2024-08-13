#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <coroutine>
#include <sys/types.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <list>
#include <queue>
#include "co_task.h"
#include "concurrentqueue.h"
#include <iostream>

using std::coroutine_handle;

template<typename DATA>
class MessageBus;

class CoExecutor;

template<typename DATA>
class MessageAwait
{
    public:
    MessageAwait(CoExecutor* co_executor, MessageBus<DATA>* message_bus, const std::string& message_name);
    ~MessageAwait();

    bool await_ready()
    {
        return queue_.try_dequeue(data_);
    }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        handle.promise().state_ = CoState::StopState;
        handle_ = handle;
    }

    DATA await_resume()
    {
        if (handle_.promise().state_ == CoState::QueueResume)
        {
            queue_.try_dequeue(data_);
        }
        handle_.promise().state_ = CoState::NormalState;
        return std::move(data_);
    }

    private:
    bool push_message(DATA data);
    friend class MessageBus<DATA>;
    std::string wait_message_name_;
    moodycamel::ConcurrentQueue<DATA> queue_;
    CoExecutor* co_executor_ = nullptr;
    MessageBus<DATA>* message_bus_ = nullptr;
    typename std::list<MessageAwait*>::const_iterator iter_;
    coroutine_handle<CoTask::promise_type> handle_;
    DATA data_;

};

template<typename DATA>
class MessageBus
{   
    public:
    MessageBus() = default;
    ~MessageBus() = default;
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;
    bool push_message(DATA&& data);
    void run();
    private:
    using IterType = typename std::list<MessageAwait<DATA>*>::const_iterator;
    CoTask dispatch_message();
    IterType add_message_await(MessageAwait<DATA>* message_await);
    void remove_message_await(IterType iter);
    friend class MessageAwait<DATA>;
    private:
    moodycamel::ConcurrentQueue<DATA> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<uint16_t> suspend_co_num_ = 0;
    std::unordered_map<std::string, std::list<MessageAwait<DATA>*>> all_message_await_;
    MessageAwait<DATA>* t;
};


using CoroutineHandlePriority = std::pair<coroutine_handle<CoTask::promise_type>, uint8_t>;
template<>
struct std::less<CoroutineHandlePriority>
{
    bool operator()( const CoroutineHandlePriority& lhs, const CoroutineHandlePriority& rhs ) const
    {
        return lhs.second < rhs.second;
    }
};

class CoExecutor
{
    public:
    CoExecutor(int thread_num):thread_num_(thread_num)
    {
    }
    void start()
    {
        if (thread_num_ < 1) thread_num_ = 1;
        thread_pool_.reserve(thread_num_);
        for (int i = 0; i < thread_num_; ++i)
        {
            thread_pool_.emplace_back([this]()
            {
                resume_coroutine();
            });
        }
    }
    ~CoExecutor()
    {
        for (auto& t : thread_pool_)
        {
            t.join();
        }
    }
    void push_coroutine(const coroutine_handle<CoTask::promise_type>& handle, uint8_t priority)
    {
        handle.promise().state_ = CoState::QueueResume;
       {
            std::lock_guard lk(mutex_);
            queue_.push({handle, priority});
            if (wait_thread_num > 0)
            {
                cv_.notify_one();
            }
       }
    }
    private:
    void resume_coroutine()
    {
        CoroutineHandlePriority co_handle;
        while (true)
        {
            std::unique_lock lk(mutex_);
            ++wait_thread_num;
            cv_.wait(lk, [this](){return queue_.size() > 0;});
            --wait_thread_num;
            co_handle = queue_.top();
            queue_.pop();
            lk.unlock();
            co_handle.first.resume();
        }
    }
    private:
    std::priority_queue<CoroutineHandlePriority> queue_;
    std::vector<std::thread> thread_pool_;
    int thread_num_;
    std::mutex mutex_;
    std::condition_variable cv_;
    uint16_t wait_thread_num = 0;
};







struct TestMessage
{
    std::string name;
    std::string data;
};
