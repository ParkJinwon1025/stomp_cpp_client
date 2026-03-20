#pragma once

class Session;

class Publisher
{
public:
    Publisher() = default;
    void HandleStarted(Session &session);
    ~Publisher() = default;
};
