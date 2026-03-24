#pragma once
#include <string>
#include <nlohmann/json.hpp>

class Session;

class Subscriber
{
public:
    Subscriber() = default;
    virtual ~Subscriber() = default;

    void dispatch(const std::string &body, Session &session); // 내부 라우터 (오버라이드 불필요)

    virtual void receive(const std::string &body, Session &session);      // 문자열이 왔을 때
    virtual void receive(const nlohmann::json &json, Session &session);   // JSON이 왔을 때
};
