#pragma once
#include <string>

class Session;

class Subscriber
{
public:
    virtual ~Subscriber() = default;
    virtual void received(const std::string &body, Session &session) = 0;
};
