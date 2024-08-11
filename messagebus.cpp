#include "messagebus.h"
#include "co_task.h"
#include <concepts>
#include <mutex>
#include <utility>




template<typename DATA>
MessageAwait<DATA>::MessageAwait(CoExecutor* co_executor, MessageBus<DATA>* message_bus, const std::string& message_name):
    co_executor_(co_executor),
    message_bus_(message_bus),
    wait_message_name_(message_name)
{
    iter_ = message_bus_->add_message_await(this);
}

template<typename DATA>
MessageAwait<DATA>::~MessageAwait()
{
    message_bus_->remove_message_await(iter_);
}
template<typename DATA>
bool MessageAwait<DATA>::push_message(DATA data)
{
    auto r = queue_.enqueue(std::move(data));
    co_executor_->post_coroutine(handle_);
    return r;
}

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
MessageBus<DATA>::IterType MessageBus<DATA>::add_message_await(MessageAwait<DATA>* message_await)
{
    auto& l =  all_message_await_[message_await->wait_message_name_];
    return l.insert(l.end(), message_await);
}
template<typename DATA>
void MessageBus<DATA>::remove_message_await(MessageBus<DATA>::IterType iter)
{
    all_message_await_[(*iter)->wait_message_name_].erase(iter);
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
CoTask MessageBus<DATA>::dispatch_message()
{
    while (true) 
    {
        DATA data;
        bool r = queue_.try_dequeue(data);
        if (r)
        {
            auto iter = all_message_await_.find(data.name);
            if (iter != all_message_await_.end())
            {
                for (const auto& a : iter->second)
                {
                    a->push_message(data);
                }
            }
        }else 
        {
            ++suspend_co_num_;
            co_await StopAwait();
            --suspend_co_num_;
        }
    }
}

template<typename DATA>
thread_local CoTask MessageBus<DATA>::co_task_;






template class MessageBus<TestMessage>;