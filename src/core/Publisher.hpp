#pragma once

class Session;

class Publisher
{
public:
    Publisher() = default;
    virtual void HandleStarted(Session &session);
    virtual ~Publisher() = default;
};
