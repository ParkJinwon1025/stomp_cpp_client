#include "Publisher.hpp"
#include "Session.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

// 스레드 실행 (detach)
void Publisher::HandleStarted(Session &session)
{
    Run(session).detach();
}

// 스레드 정의 — 사용자가 직접 로직을 구현하고 반환
std::thread Publisher::Run(Session &session)
{
    return std::thread([&session]()
                       {
        while (true)
        {
            // duration_cast : 시간을 밀리초 단위로 변환
            // std::chrono::system_clock::now().time_since_epoch() : 1970년 1월 1일부터 지금까지의 시간
            // count 숫자로 꺼내기
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            // 빈 json 객체 생성
            nlohmann::json j;

            j["type"] = "report"; // type 키에 "report" 추가
            j["timestamp"] = ms; // timestamp 키에 ms 값 추가

            LOG("[PUBLISHER3] json object -> " << j.dump());
            session.Send("/app/ubisam", j);

            std::this_thread::sleep_for(std::chrono::seconds(1));
        } });
}
