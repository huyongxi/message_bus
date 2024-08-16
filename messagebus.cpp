#include "messagebus.h"


template<typename T>
SharedMessageAwait<T>::SharedMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, 
    const std::string& wait_message_name, std::shared_ptr<moodycamel::ConcurrentQueue<T>> queue):
    message_bus_(message_bus),
    co_executor_(co_executor),
    wait_message_name_(wait_message_name)
{
    if (queue)
    {
        queue_ = queue;
    }else 
    {
        queue_ = std::make_shared<moodycamel::ConcurrentQueue<T>>();
    }
    iter_ = message_bus_->add_await(this);
}

template<typename T>
SharedMessageAwait<T>::~SharedMessageAwait()
{
    message_bus_->template remove_await<SharedMessageAwait<T>>(iter_);
}


template<typename T>
MessageAwait<T>::MessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name):
    message_bus_(message_bus),
    co_executor_(co_executor),
    wait_message_name_(wait_message_name)
{
    iter_ = message_bus_->add_await(this);
}

template<typename T>
MessageAwait<T>::~MessageAwait()
{
    message_bus_-> template remove_await<MessageAwait<T>>(iter_);
}


template<typename T>
OnceMessageAwait<T>::OnceMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name):
    message_bus_(message_bus),
    co_executor_(co_executor),
    wait_message_name_(wait_message_name)
{
    iter_ = message_bus_->add_await(this);
}

template<typename T>
OnceMessageAwait<T>::~OnceMessageAwait()
{
    message_bus_-> template remove_await<OnceMessageAwait<T>>(iter_);
}


template class SharedMessageAwait<TestMessage>;
template class MessageAwait<TestMessage>;
template class OnceMessageAwait<TestMessage>;
