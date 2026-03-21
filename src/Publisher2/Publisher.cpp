#include "Publisher.hpp"
#include "Session.hpp"
#include <thread>
#include <chrono>
#include <string>

void Publisher::HandleStarted(Session &session)
{
    Run(session).detach();
}

std::thread Publisher::Run(Session &session)
{
    return std::thread([&session]()
    {
        while (true)
        {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            session.Send("/app/ubisam", "{\"timestamp\":" + std::to_string(ms) + "}");

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}
