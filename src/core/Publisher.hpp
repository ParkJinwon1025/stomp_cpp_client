#pragma once
#include <thread>

class Session;

class Publisher
{
public:
    Publisher() = default;
    void HandleStarted(Session &session); // 스레드 실행 (detach)
    ~Publisher() = default;

private:
    std::thread Run(Session &session); // 스레드 정의 — 사용자가 직접 로직을 구현하고 반환
};
