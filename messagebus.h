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
#include "co_task.h"
#include "concurrentqueue.h"
#include "blockingconcurrentqueue.h"

using std::coroutine_handle;
using std::suspend_always;
using std::suspend_never;

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
        suspend_ = true;
        handle_ = handle;
    }

    DATA await_resume()
    {
        suspend_ = false;
        return std::move(data_);
    }

    private:
    bool push_message(DATA data);
    friend class MessageBus<DATA>;
    std::string wait_message_name_;
    moodycamel::ConcurrentQueue<DATA> queue_;
    CoExecutor* co_executor_ = nullptr;
    MessageBus<DATA>* message_bus_ = nullptr;
    std::list<MessageAwait*>::const_iterator iter_;
    bool suspend_ = false;
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
    using IterType = std::list<MessageAwait<DATA>*>::const_iterator;
    CoTask dispatch_message();
    IterType add_message_await(MessageAwait<DATA>* message_await);
    void remove_message_await(IterType iter);
    friend class MessageAwait<DATA>;
    private:
    moodycamel::ConcurrentQueue<DATA> queue_;
    static thread_local CoTask co_task_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<uint16_t> suspend_co_num_ = 0;
    std::unordered_map<std::string, std::list<MessageAwait<DATA>*>> all_message_await_;
};



class CoExecutor
{
    public:
    CoExecutor(int thread_num)
    {
        if (thread_num < 2) thread_num = 2;
        thread_pool_.reserve(thread_num);
        int h = int(thread_num*0.8);
        for (int i = 0; i < h; ++i)
        {
            thread_pool_.emplace_back([this]()
            {
                run(high_priority_queue_);
            });
        }
        for (int i = 0; i < (thread_num-h); ++i)
        {
            thread_pool_.emplace_back([this]()
            {
                run(low_priority_queue_);
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

    void post_coroutine(coroutine_handle<CoTask::promise_type> handle, bool high_priority = true)
    {
        if (high_priority)
        {
            high_priority_queue_.enqueue(handle);
        }else 
        {
            low_priority_queue_.enqueue(handle);
        }
    }
    private:
    void run(moodycamel::BlockingConcurrentQueue<coroutine_handle<CoTask::promise_type>>& queue)
    {
        coroutine_handle<CoTask::promise_type> handle;
        while (true) 
        {
            queue.wait_dequeue(handle);
            handle.resume();
        }
    }
    moodycamel::BlockingConcurrentQueue<coroutine_handle<CoTask::promise_type>> high_priority_queue_;
    moodycamel::BlockingConcurrentQueue<coroutine_handle<CoTask::promise_type>> low_priority_queue_;
    std::vector<std::thread> thread_pool_;
};



struct TestMessage
{
    std::string name;
    std::string data;
};
