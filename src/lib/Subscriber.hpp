#pragma once
#include <string>
#include <nlohmann/json.hpp>

class Session;

class Subscriber
{
public:
    Subscriber() = default;
    virtual ~Subscriber() = default;

    virtual void receive(Session &session, const nlohmann::json &json); // JSON이 왔을 때
};
