#include "Reconnector.hpp"
#include "Session.hpp"
#include <chrono>
#include <thread>

// intervalSec : 재연결 시도 전 대기 시간
Reconnector::Reconnector(Session &session, int intervalSec)
    : session(session), intervalSec(intervalSec) {}

// 소멸자
Reconnector::~Reconnector() { Stop(); } // 소멸 시 자동 종료

void Reconnector::Start()
{
    // 감시 루프 플래그 On
    running = true;

    // Run()을 별도 스레드에서 실행
    thread = std::thread([this]()
                         { Run(); }); // 감시 스레드 시작
}

void Reconnector::Stop()
{
    // 감시 루프 플래그 OFF
    running = false;
    if (thread.joinable()) // 스레드가 실행 중인지 확인
        thread.join();     // 스레드가 완전히 끝날 때까지 대기
}

void Reconnector::Run()
{

    // running이 true일 동안 계속 반복
    while (running)
    {
        if (!session.IsConnected()) // 연결이 끊겼을 때만 재연결 시도
        {
            LOG("[RECONNECTOR] Connection lost -> Reconnecting in "
                << intervalSec << "s...");
            // 재연결 전 대기 (연결 끊기자마자 바로 연결하면 문제가 생길 수 있음.)
            std::this_thread::sleep_for(std::chrono::seconds(intervalSec));

            // 대기 후 Stop()이 불린 상태면 루프 탈출 (종료 중에 재연결을 시도하면 안되기 때문)
            if (!running)
                break;

            // 대기 후 이미 연결됐으면 스킵
            if (session.IsConnected())
                continue;

            LOG("[RECONNECTOR] Trying to reconnect...");
            session.Connect(); // 재연결 시도

            // 대기 횟수
            // 0.1초 X 50번 = 5초
            // Connect가 비동기라 결과를 기다리기
            int waited = 0;
            // 연결 안됨 / 50번 미만 / 실행 중
            while (!session.IsConnected() && waited < 50 && running)
            {
                // 100ms 대기
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // 대기 횟수 증가
                waited++;
            }

            // 5초 안에 연결됐으면
            if (session.IsConnected())
            {
                LOG("[RECONNECTOR] Reconnected successfully");
            }
            else
            {
                LOG("[RECONNECTOR] Reconnect failed -> retrying...");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 500ms마다 연결 상태 체크
    }
}
