#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "coroutine.h"
#include <sys/types.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <list>
#include <optional>
#include <queue>
#include <iostream>
#include "concurrentqueue.h"
#include "co_task.h"

using std::coroutine_handle;

using CoHandleWithPriority = std::pair<coroutine_handle<CoTask::promise_type>, uint8_t>;
template<>
struct std::less<CoHandleWithPriority>
{
    bool operator()( const CoHandleWithPriority& lhs, const CoHandleWithPriority& rhs ) const
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
                loop_resume_coroutine();
            });
        }
    }

    void stop()
    {
        {
            std::lock_guard lk(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
    }

    ~CoExecutor()
    {
        for (auto& t : thread_pool_)
        {
            t.join();
        }
    }

    bool resume_coroutine(const coroutine_handle<CoTask::promise_type>& handle, uint8_t priority = 0)
    {
         if (handle.promise().state_ == CoState::StopState)
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
            return true;
         }else 
         {
            return false;
         }
    }

    private:
    void loop_resume_coroutine()
    {
        CoHandleWithPriority co_handle;
        while (true)
        {
            std::unique_lock lk(mutex_);
            ++wait_thread_num;
            cv_.wait(lk, [this](){return queue_.size() > 0 || stop_;});
            --wait_thread_num;
            if (stop_)
            {
                lk.unlock();
                return;
            }
            co_handle = queue_.top();
            queue_.pop();
            lk.unlock();
            co_handle.first.resume();
        }
    }
    private:
    std::priority_queue<CoHandleWithPriority> queue_;
    std::vector<std::thread> thread_pool_;
    int thread_num_;
    std::mutex mutex_;
    std::condition_variable cv_;
    uint16_t wait_thread_num = 0;
    bool stop_ = false;
};


template<typename T>
class MessageBus;

template<typename T>
class SharedMessageAwait
{
    public:
    bool await_ready()
    {
        return queue_->try_dequeue(data_);
    }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        suspend_ = true;
        handle_ = handle;
        handle_.promise().state_ = CoState::StopState;
    }

    T&& await_resume()
    {
        if (suspend_)
        {
            queue_->try_dequeue(data_);
        }   
        suspend_ = false;
        handle_.promise().state_ = CoState::NormalState;
        return std::move(data_);
    }
    ~SharedMessageAwait();
    private:
    using IterType = typename std::list<SharedMessageAwait<T>*>::const_iterator;
    SharedMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name, 
        std::shared_ptr<moodycamel::ConcurrentQueue<T>> queue = nullptr);
    SharedMessageAwait<T> clone()
    {
        return {message_bus_, co_executor_, wait_message_name_, queue_};
    }

    bool push_message(T data)
    {
        return queue_->enqueue(std::move(data));
    }
    bool resume_wait_coroutine()
    {
        return co_executor_->resume_coroutine(handle_);
    }
    friend class MessageBus<T>;
    std::string wait_message_name_;
    std::shared_ptr<moodycamel::ConcurrentQueue<T>> queue_;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    coroutine_handle<CoTask::promise_type> handle_;
    bool suspend_ = false;
    T data_;
    IterType iter_;
};
template<typename T>
class MessageAwait
{
    public:
    bool await_ready()
    {
        return queue_.try_dequeue(data_);
    }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        suspend_ = true;
        handle_ = handle;
        handle_.promise().state_ = CoState::StopState;
    }

    T&& await_resume()
    {
        if (suspend_)
        {
            queue_.try_dequeue(data_);
        }   
        suspend_ = false;
        handle_.promise().state_ = CoState::NormalState;
        return std::move(data_);
    }

    ~MessageAwait();
    private:
    using IterType = typename std::list<MessageAwait<T>*>::const_iterator;
    
    MessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name);

    bool push_message(T data)
    {
        bool r = queue_.enqueue(std::move(data));
        co_executor_->resume_coroutine(handle_);
        return r;
    }
    friend class MessageBus<T>;
    std::string wait_message_name_;
    moodycamel::ConcurrentQueue<T> queue_;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    coroutine_handle<CoTask::promise_type> handle_;
    bool suspend_ = false;
    T data_;
    IterType iter_;
};
template<typename T>
class OnceMessageAwait
{
    public:
    bool await_ready()
    {
        return false;
    }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        handle_ = handle;
        handle_.promise().state_ = CoState::StopState;
    }

    T&& await_resume()
    {
        handle_.promise().state_ = CoState::NormalState;
        return std::move(data_);
    }
    ~OnceMessageAwait();
    private:
    using IterType = typename std::list<OnceMessageAwait<T>*>::const_iterator;
    OnceMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name);

    bool push_message(T data)
    {
        data_ = std::move(data);
        co_executor_->resume_coroutine(handle_);
        return true;
    }
    friend class MessageBus<T>;
    std::string wait_message_name_;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    coroutine_handle<CoTask::promise_type> handle_;
    T data_;
    IterType iter_;
};


enum class AwaitType : uint8_t
{
    Invalid = 0,
    Shared = 1,
    Once = 2,
    Normal = 3,
};

template<typename T>
struct AwaitTypeTraits
{
    const static AwaitType value = AwaitType::Invalid;
};


template<typename T>
struct AwaitTypeTraits<SharedMessageAwait<T>>
{
    const static AwaitType value = AwaitType::Shared;
};
template<typename T>
struct AwaitTypeTraits<MessageAwait<T>>
{
    const static AwaitType value = AwaitType::Normal;
};
template<typename T>
struct AwaitTypeTraits<OnceMessageAwait<T>>
{
    const static AwaitType value = AwaitType::Once;
};


template<typename T>
class MessageBus
{   
    public:
    MessageBus() = default;
    ~MessageBus() = default;
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;

    SharedMessageAwait<T> create_shared_message_await(CoExecutor* co_executor, const std::string& wait_message_name)
    {
        auto it = shared_message_await_map_.find(wait_message_name);
        if (it != shared_message_await_map_.end() && it->second.size() > 0)
        {
            return it->second.front()->clone();
        }
        return {this, co_executor, wait_message_name};
    }
    MessageAwait<T> create_message_await(CoExecutor* co_executor, const std::string& wait_message_name)
    {
        return {this, co_executor, wait_message_name};
    }
    OnceMessageAwait<T> create_once_message_await(CoExecutor* co_executor, const std::string& wait_message_name)
    {
        return {this, co_executor, wait_message_name};
    }

    template<typename Await>
    typename Await::IterType add_await(Await* await)
    {
        if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Shared)
        {
            shared_message_await_map_[await->wait_message_name_].push_back(await);
            return --shared_message_await_map_[await->wait_message_name_].end();
        }
        else if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Normal)
        {
            message_await_map_[await->wait_message_name_].push_back(await);
            return --message_await_map_[await->wait_message_name_].end();
        }
        else if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Once)
        {
            once_message_await_map_[await->wait_message_name_].push_back(await);
            return --once_message_await_map_[await->wait_message_name_].end();
        }
        assert(false);
        return {};
    }
    template<typename Await>
    void remove_await(typename Await::IterType iter)
    {
        if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Shared)
        {
            shared_message_await_map_[(*iter)->wait_message_name_].erase(iter);
        }
        else if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Normal)
        {
            message_await_map_[(*iter)->wait_message_name_].erase(iter);
        }
        else if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Once)
        {
            once_message_await_map_[(*iter)->wait_message_name_].erase(iter);
        }
        return;
    }

    bool push_message(T&& data)
    {
        bool r = queue_.enqueue(std::move(data));
        if (suspend_co_num_ > 0 && r)
        {
            cv_.notify_one();
        }
        return r;
    }

    void run()
    {
        CoTask co_task = dispatch_message();
        while (!stop_)
        {
            std::unique_lock lk(mutex_);
            cv_.wait(lk, [this](){return queue_.size_approx() > 0 || stop_;});
            lk.unlock();
            co_task.resume();
        }
    }
    void stop()
    {
        stop_ = true;
        cv_.notify_all();
    }
    private:
    CoTask dispatch_message()
    {
        while (!stop_) 
        {
            T data;
            bool r = queue_.try_dequeue(data);
            if (r)
            {
                {
                    auto it = shared_message_await_map_.find(data.name);
                    if (it != shared_message_await_map_.end() && it->second.size() > 0)
                    {
                        it->second.front()->push_message(data);
                        for (auto& await : it->second)
                        {
                            if (await->resume_wait_coroutine())
                                break;;
                        }
                    }
                }
                {
                    auto it = message_await_map_.find(data.name);
                    if (it != message_await_map_.end() && it->second.size() > 0)
                    {
                        for (auto& await : it->second)
                        {
                            await->push_message(data);
                        }
                        
                    }
                }
                {
                    auto it = once_message_await_map_.find(data.name);
                    if (it != once_message_await_map_.end() && it->second.size() > 0)
                    {
                        for (auto& await : it->second)
                        {
                            await->push_message(data);
                        }
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

    private:
    moodycamel::ConcurrentQueue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<uint16_t> suspend_co_num_ = 0;
    std::atomic<bool> stop_ = false;
    std::unordered_map<std::string, std::list<SharedMessageAwait<T>*>> shared_message_await_map_;
    std::unordered_map<std::string, std::list<MessageAwait<T>*>> message_await_map_;
    std::unordered_map<std::string, std::list<OnceMessageAwait<T>*>> once_message_await_map_;
};






struct TestMessage
{
    std::string name;
    std::string data;
};
