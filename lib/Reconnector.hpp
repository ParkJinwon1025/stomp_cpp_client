#pragma once
#include <thread>
#include <atomic>

class Session;

// 연결 상태를 감시하며 끊겼을 때 자동으로 재연결을 시도하는 클래스
class Reconnector
{
public:
    Reconnector(Session &session, int intervalSec = 5); // intervalSec: 재연결 시도 간격 (초)
    ~Reconnector();

    void Start(); // 감시 스레드 시작
    void Stop();  // 감시 스레드 종료

private:
    Session &session;
    int intervalSec;                  // 재연결 대기 간격 (초)
    std::atomic<bool> running{false}; // 스레드 실행 플래그
    std::thread thread;               // 감시 스레드

    void Run(); // 스레드 본체: isConnected 폴링 → 끊기면 Connect 호출
};
