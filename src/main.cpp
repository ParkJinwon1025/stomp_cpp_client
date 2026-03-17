#include "StompInterface.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    const std::string url = "ws://localhost:9030/stomp/websocket";

    StompInterface::getInstance().init(
        {[]()
         { std::cout << "[INFO] server connected.\n"; },
         []()
         { std::cout << "[INFO] server disconnected.\n"; }});

    auto &stomp = StompInterface::getInstance();

    std::cout << "[TEST] Connection_Start()" << std::endl;
    stomp.Connection_Start(url);

    int waited = 0;
    while (!stomp.Connection_IsAlive() && waited < 50)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        waited++;
    }

    std::cout << "[TEST] Connection_IsAlive(): " << (stomp.Connection_IsAlive() ? "true" : "false") << std::endl;

    if (stomp.Connection_IsAlive())
    {
        std::cout << "[TEST] Message_Subscribe()" << std::endl;
        stomp.Message_Subscribe("/topic/robot", [&](const std::string &destination, const std::string &body)
                                {
            std::cout << "[received] destination: " << destination << std::endl;
            std::cout << "[received] body: " << body << std::endl;

            std::cout << "[TEST] Message_Publish()" << std::endl;
            stomp.Message_Publish("/app/ubisam", "{\"type\":\"response\",\"message\":\"ok\"}"); });

        std::cout << "[TEST] Message_Publish() 직접 호출" << std::endl;
        stomp.Message_Publish("/app/ubisam", "{\"type\":\"status\",\"message\":\"hello\"}");

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    std::cout << "[TEST] Connection_End()" << std::endl;
    stomp.Connection_End();

    std::cout << "[TEST] Connection_IsAlive() after end: " << (stomp.Connection_IsAlive() ? "true" : "false") << std::endl;

    return 0;
}