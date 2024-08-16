#include "messagebus.h"
#include <iostream>
#include <chrono>


CoTask test_msg1(MessageBus<TestMessage>* message_bus, CoExecutor* co_executor, int id)
{
    auto await = message_bus->create_shared_message_await(co_executor, "msg1");
    while (true) 
    {
        TestMessage msg = co_await await;
        std::cout << std::this_thread::get_id() << " " << id << " msg1 " << msg.data << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


CoTask test_msg2(MessageBus<TestMessage>* message_bus, CoExecutor* co_executor)
{
    auto await = message_bus->create_message_await(co_executor, "msg2");
    while (true) 
    {
        TestMessage msg = co_await await;
        std::cout << "msg2 " << msg.data << std::endl;
    }
}

CoTask test_msg3(MessageBus<TestMessage>* message_bus, CoExecutor* co_executor)
{
    while (true) 
    {
        TestMessage msg = co_await message_bus->create_once_message_await(co_executor, "msg3");
        std::cout << "msg3 " << msg.data << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

int main()
{

    auto message_bus = std::make_shared<MessageBus<TestMessage>>();
    auto co_executor = std::make_shared<CoExecutor>(3);
    co_executor->start();
    test_msg1(message_bus.get(), co_executor.get(), 1);
    test_msg1(message_bus.get(), co_executor.get(), 2);
    test_msg1(message_bus.get(), co_executor.get(), 3);
    test_msg2(message_bus.get(), co_executor.get());
    test_msg3(message_bus.get(), co_executor.get());


    moodycamel::ConcurrentQueue<TestMessage> q;
    
    std::thread t1([&]()
    {
        int i = 0;
        while (true)
        {
            TestMessage msg;
            msg.name = "msg1";
            msg.data = std::to_string(++i);
            auto r = message_bus->push_message(std::move(msg));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::thread t2([=]()
    {
        int i = 0;
        while (true)
        {
            TestMessage msg;
            msg.name = "msg2";
            msg.data = std::to_string(++i);
            message_bus->push_message(std::move(msg));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });

    std::thread t3([=]()
    {
        int i = 0;
        while (true)
        {
            TestMessage msg;
            msg.name = "msg3";
            msg.data = std::to_string(++i);
            message_bus->push_message(std::move(msg));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });

    std::thread t4([=](){message_bus->run();});
    message_bus->run();
    
    t4.join();

    t1.join();
    t2.join();
    t3.join();
}