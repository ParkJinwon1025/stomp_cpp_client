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

    std::cout << "[TEST] start()" << std::endl;
    stomp.start(url);

    int waited = 0;
    while (!stomp.isConnected() && waited < 50)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        waited++;
    }

    std::cout << "[TEST] isConnected(): " << (stomp.isConnected() ? "true" : "false") << std::endl;

    if (stomp.isConnected())
    {
        std::cout << "[TEST] sub()" << std::endl;
        stomp.sub("/topic/robot", [&](const std::string &destination, const std::string &body)
                  {
            std::cout << "[received] destination: " << destination << std::endl;
            std::cout << "[received] body: " << body << std::endl;

            std::cout << "[TEST] pub()" << std::endl;
            stomp.pub("/app/ubisam", "{\"type\":\"response\",\"message\":\"ok\"}"); });

        std::cout << "[TEST] pub() 직접 호출" << std::endl;
        stomp.pub("/app/ubisam", "{\"type\":\"status\",\"message\":\"hello\"}");

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    std::cout << "[TEST] stop()" << std::endl;
    stomp.stop();

    std::cout << "[TEST] isConnected() after stop: " << (stomp.isConnected() ? "true" : "false") << std::endl;

    return 0;
}