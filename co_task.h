#pragma once
#include <cstdint>
#include <exception>
#include "coroutine.h"
#include <atomic>

using std::coroutine_handle;
using std::suspend_always;
using std::suspend_never;


enum class CoState : uint8_t
{
    NormalState = 0,
    ExceptionState = 1,
    StopState = 2,
    QueueResume = 3,
    TimeoutState = 4,
};


class CoTask
{
    public:
    void resume()
    {
        coroutine_handle<CoTask::promise_type>::from_promise(promise_).resume();
    }

    class promise_type
    {
        public:
        CoTask get_return_object()
        {
            return {*this};
        }

        suspend_never initial_suspend()
        {
            return {};
        }

        void return_void()
        {

        }
        suspend_always yield_value(int)
        {
            return {};
        }

        void unhandled_exception()
        {
            exception_ = std::current_exception();
        }

        suspend_never final_suspend() noexcept
        {
            return {};
        }

        // template <typename U>
        // U&& await_transform(U&& awaitable) noexcept
        // {
        //     return static_cast<U&&>(awaitable);
        // }

        std::exception_ptr exception_;
        std::atomic<CoState> state_{CoState::NormalState};
        void* await = nullptr;
    };
    promise_type& promise_;
};

class StopAwait
{
    public:

    bool await_ready()
    {
        return false;
    }

    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        handle.promise().state_ = CoState::StopState;
        handle_ = handle;
    }

    void await_resume()
    {
        handle_.promise().state_ = CoState::NormalState;
    }

    private:
    coroutine_handle<CoTask::promise_type> handle_;
};