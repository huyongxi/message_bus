#include "messagebus.h"
#include "co_task.h"
#include <mutex>






template<typename DATA>
bool MessageBus<DATA>::push_message(DATA&& data)
{
    bool r = queue_.enqueue(std::move(data));
    if (suspend_co_num_ > 0 && r)
    {
        cv_.notify_one();
    }
    return r;
}



template<typename DATA>
void MessageBus<DATA>::run()
{
    co_task_ = dispatch_message();
    while (true)
    {
        std::unique_lock lk(mutex_);
        cv_.wait(lk, [this](){return queue_.size_approx() > 0;});
        lk.unlock();
        co_task_.resume();
    }
}

template<typename DATA>
Co_Task MessageBus<DATA>::dispatch_message()
{
    while (true) 
    {
        DATA data;
        bool r = queue_.try_dequeue(data);
        if (r)
        {

        }else 
        {
            ++suspend_co_num_;
            co_await StopAwait();
            --suspend_co_num_;
        }
    }
}

template<typename DATA>
thread_local Co_Task MessageBus<DATA>::co_task_;





template class MessageBus<int>;
template class MessageBus<TestMessage>;