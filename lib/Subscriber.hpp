#pragma once
#include <nlohmann/json.hpp>

class Session;

class Subscriber
{
public:
    Subscriber() = default;
    ~Subscriber() = default;

    void HandleReceived(Session &session, const nlohmann::json &json); // 메시지 수신 시 호출 → Run 실행

private:
    void Run(Session &session, const nlohmann::json &json); // 사용자가 직접 로직 구현
};
