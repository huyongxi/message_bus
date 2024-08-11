#pragma once
#include <mutex>
#include <condition_variable>
#include <coroutine>
#include "co_task.h"
#include "concurrentqueue.h"

using std::coroutine_handle;
using std::suspend_always;
using std::suspend_never;



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
    Co_Task dispatch_message();

    private:
    moodycamel::ConcurrentQueue<DATA> queue_;
    static thread_local Co_Task co_task_;
    std::mutex mutex_;
    std::condition_variable cv_;
};