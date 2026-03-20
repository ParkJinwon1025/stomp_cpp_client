#pragma once

class Session;

class Publisher
{
public:
    Publisher(Session &session);
    virtual void run();
    virtual ~Publisher() = default;

protected:
    Session &session;
};
