#include "Reconnector.hpp"
#include "Session.hpp"
#include <chrono>
#include <thread>

Reconnector::Reconnector(Session &session, int intervalSec)
    : session(session), intervalSec(intervalSec) {}

Reconnector::~Reconnector() { stop(); }

void Reconnector::start()
{
    running = true;
    thread = std::thread([this]()
                         { loop(); });
}

void Reconnector::stop()
{
    running = false;
    if (thread.joinable())
        thread.join();
}

void Reconnector::loop()
{
    while (running)
    {
        if (!session.isConnected())
        {
            LOG("[RECONNECTOR] Connection lost -> Reconnecting in "
                << intervalSec << "s...");
            std::this_thread::sleep_for(std::chrono::seconds(intervalSec));

            if (!running)
                break;

            LOG("[RECONNECTOR] Trying to reconnect...");
            session.connect();

            int waited = 0;
            while (!session.isConnected() && waited < 50 && running)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                waited++;
            }

            if (session.isConnected())
            {
                LOG("[RECONNECTOR] Reconnected successfully");
            }
            else
            {
                LOG("[RECONNECTOR] Reconnect failed -> retrying...");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
