#include "Publisher.hpp"
#include "Session.hpp"
#include <string>
#include <iostream>

// 스레드 실행 (join — HandleStarted가 블로킹됨)
void Publisher::HandleStarted(Session &session)
{
    Run(session).join();
}

// 스레드 정의 — stdin 입력을 받아 Send
std::thread Publisher::Run(Session &session)
{
    return std::thread([&session]()
                       {
        std::string message;
        while (true)
        {
            std::cout << "message (q to quit): " << std::flush;
            std::getline(std::cin, message);

            if (message == "q")
                break;

            session.Publish("/app/ubisam", "{\"payload\":\"" + message + "\"}");
        } });
}
