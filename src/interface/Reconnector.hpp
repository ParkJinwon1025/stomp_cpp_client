#pragma once
#include <thread>
#include <atomic>

class Session;

class Reconnector
{
public:
    Reconnector(Session &session, int intervalSec = 5);
    ~Reconnector();

    void start();
    void stop();

private:
    Session &session;
    int intervalSec;
    std::atomic<bool> running{false};
    std::thread thread;

    void loop();
};
