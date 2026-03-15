#include "StompInterface.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

void reconnectLoop(
    StompInterface &stomp,
    std::atomic<bool> &stopRequested,
    int reconnectDelaySec = 3)
{
    auto doSubscribe = [&]()
    {
        stomp.sub("/topic/robot", [&](const std::string &destination, const std::string &body)
                  {
            std::cout << "[received] destination: " << destination << std::endl;
            std::cout << "[received] body: " << body << std::endl;
            stomp.pub("/app/ubisam", "{\"type\":\"response\",\"message\":\"ok\"}"); });
    };

    while (!stopRequested)
    {
        // 연결 시도
        stomp.start();

        // 연결될 때까지 최대 5초 대기
        int waited = 0;
        while (!stomp.isConnected() && !stopRequested && waited < 50)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited++;
        }

        // 연결 성공
        if (stomp.isConnected())
        {
            doSubscribe();

            // 끊길 때까지 대기
            while (stomp.isConnected() && !stopRequested)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (stopRequested)
            return;

        // 카운트다운
        for (int i = reconnectDelaySec; i > 0; i--)
        {
            if (stopRequested)
                return;
            std::cout << "[reconnecting...] " << i << "s" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void inputLoop(
    StompInterface &stomp,
    std::atomic<bool> &stopRequested)
{
    std::string input;
    std::cout << "enter message (quit to exit)\n"
              << std::endl;

    while (true)
    {
        std::getline(std::cin, input);
        if (input.empty())
            continue;

        if (input == "quit")
        {
            stopRequested = true;
            stomp.stop();
            break;
        }

        if (!stomp.isConnected())
        {
            std::cout << "[warning] not connected.\n";
            continue;
        }

        std::string body;
        if (!input.empty() && input.front() == '{')
            body = input;
        else
            body = "{\"type\":\"status\",\"message\":\"" + input + "\"}";

        std::cout << "[send] destination: /app/ubisam" << std::endl;
        std::cout << "[send] body: " << body << std::endl;
        stomp.pub("/app/ubisam", body);
    }
}

int main()
{
    std::atomic<bool> stopRequested{false};

    StompInterface stomp(
        "ws://localhost:9030/stomp/websocket",
        {[]()
         { std::cout << "[INFO] server connected.\n"; },
         []()
         { std::cout << "[INFO] server disconnected.\n"; }});

    std::thread reconnectThread([&]()
                                { reconnectLoop(stomp, stopRequested); });

    inputLoop(stomp, stopRequested);

    if (reconnectThread.joinable())
        reconnectThread.join();

    return 0;
}