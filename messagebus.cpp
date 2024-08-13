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
    static int c = 0;
    ++c;
    if (c % 10000 == 0)
    {
        std::cout << "await push " << c << std::endl;
    }
    auto r = queue_.enqueue(std::move(data));
    if (handle_.promise().state_ == CoState::StopState)
    {
        co_executor_->push_coroutine(handle_, 10);
    }
    return r;
}

template<typename DATA>
bool MessageBus<DATA>::push_message(DATA&& data)
{
    static int i = 0;
    bool r = queue_.enqueue(std::move(data));
    if (r && ++i % 10000 == 0)
    {
        std::cout << "m push " << i << std::endl;
    }

    if (suspend_co_num_ > 0 && r)
    {
        cv_.notify_one();
    }
    return r;
}


template<typename DATA>
typename MessageBus<DATA>::IterType MessageBus<DATA>::add_message_await(MessageAwait<DATA>* message_await)
{
    t = message_await;
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
    CoTask co_task = dispatch_message();
    while (true)
    {
        std::unique_lock lk(mutex_);
        cv_.wait(lk, [this](){return queue_.size_approx() > 0;});
        lk.unlock();
        co_task.resume();
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
            static std::atomic<int> c = 0;
            ++c;
            if (c % 10000 == 0)
            {
                std::cout << "dispatch push " << c << std::endl;
            }
            t->push_message(data);
            // auto iter = all_message_await_.find(data.name);
            // if (iter != all_message_await_.end())
            // {
            //     for (const auto& a : iter->second)
            //     {
            //         a->push_message(data);
            //     }
            // }
        }else 
        {
            ++suspend_co_num_;
            //co_await StopAwait();
            --suspend_co_num_;
        }
    }
}






template class MessageBus<TestMessage>;
template class MessageAwait<TestMessage>;