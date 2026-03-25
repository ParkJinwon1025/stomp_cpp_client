#include "Reconnector.hpp"
#include "Session.hpp"
#include <chrono>
#include <thread>

Reconnector::Reconnector(Session &session, int intervalSec)
    : session(session), intervalSec(intervalSec) {}

Reconnector::~Reconnector() { Stop(); } // 소멸 시 자동 종료

void Reconnector::Start()
{
    running = true;
    thread = std::thread([this]()
                         { Run(); }); // 감시 스레드 시작
}

void Reconnector::Stop()
{
    running = false;
    if (thread.joinable())
        thread.join(); // 스레드 종료 대기
}

void Reconnector::Run()
{
    while (running)
    {
        if (!session.IsConnected()) // 연결이 끊겼을 때만 재연결 시도
        {
            LOG("[RECONNECTOR] Connection lost -> Reconnecting in "
                << intervalSec << "s...");
            std::this_thread::sleep_for(std::chrono::seconds(intervalSec)); // 재연결 전 대기

            if (!running)
                break;

            if (session.IsConnected()) // 대기 중에 이미 연결됐으면 스킵
                continue;

            LOG("[RECONNECTOR] Trying to reconnect...");
            session.Connect(); // 재연결 시도

            // 최대 5초(50 * 100ms) 동안 연결 확인 대기
            int waited = 0;
            while (!session.IsConnected() && waited < 50 && running)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                waited++;
            }

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
