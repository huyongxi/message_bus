#include "messagebus.h"
#include <iostream>
#include <chrono>


CoTask test_msg1(MessageBus<TestMessage>* message_bus, CoExecutor* co_executor)
{
    auto await = MessageAwait<TestMessage>(co_executor, message_bus, "msg1");
    uint64_t c = 0;
    while (true) 
    {
        TestMessage msg = co_await await;
        ++c;
        ///std::cout << "msg1 " << msg.data << std::endl;
        if (c % 10000 == 0)
        {
            std::cout << " get " << c << std::endl;
        }
    }
}


CoTask test_msg2(MessageBus<TestMessage>* message_bus, CoExecutor* co_executor)
{
    auto await = MessageAwait<TestMessage>(co_executor, message_bus, "msg2");
    while (true) 
    {
        TestMessage msg = co_await await;
        //std::cout << "msg2 " << msg.data << std::endl;
    }
}

CoTask test_msg3(MessageBus<TestMessage>* message_bus, CoExecutor* co_executor)
{
    auto await = MessageAwait<TestMessage>(co_executor, message_bus, "msg3");
    while (true) 
    {
        TestMessage msg = co_await await;
        //std::cout << "msg3 " << msg.data << std::endl;
    }
}

int main()
{

    auto message_bus = std::make_shared<MessageBus<TestMessage>>();
    auto co_executor = std::make_shared<CoExecutor>(2);
    co_executor->start();
    test_msg1(message_bus.get(), co_executor.get());
    //test_msg2(message_bus.get(), co_executor.get());
    //test_msg3(message_bus.get(), co_executor.get());


    moodycamel::ConcurrentQueue<TestMessage> q;
    
    std::thread t1([&]()
    {
        int i = 0;
        while (true)
        {
            TestMessage msg;
            msg.name = "msg1";
            msg.data = std::to_string(i);
            auto r = message_bus->push_message(std::move(msg));
            if (r && ++i % 10000 == 0)
            {
                std::cout << "push " << i << std::endl;
            }
        }
    });

    std::thread t2([=]()
    {
        int i = 0;
        while (true)
        {
            ++i;
            TestMessage msg;
            msg.name = "msg2";
            msg.data = std::to_string(i);
            message_bus->push_message(std::move(msg));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    std::thread t3([=]()
    {
        int i = 0;
        while (true)
        {
            ++i;
            TestMessage msg;
            msg.name = "msg3";
            msg.data = std::to_string(i);
            message_bus->push_message(std::move(msg));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    //std::thread t4([=](){message_bus->run();});
    message_bus->run();
    
    //t4.join();

    t1.join();
    t2.join();
    t3.join();
}