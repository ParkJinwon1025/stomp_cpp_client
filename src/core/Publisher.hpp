#pragma once
#include <thread>

class Session;

class Publisher
{
public:
    Publisher() = default;
    void HandleStarted(Session &session);
    ~Publisher() = default;

private:
    std::thread Run(Session &session);
};
