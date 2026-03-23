#include "Publisher.hpp"
#include "Session.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

// 사용자 구현 구조체 예시
struct TimestampData
{
    std::string type;
    long long timestamp;
};
// struct → json 자동 변환 등록 (이 매크로 없으면 nlohmann::json j = data 에서 컴파일 에러)
// JSON 변환 struct 및 필드 명시
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TimestampData, type, timestamp)

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
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            // type: "report", timestamp : ms로 초기화
            TimestampData data{ "report", ms };
            nlohmann::json j = data; // struct → json 변환
            LOG("[PUBLISHER4] struct -> json -> " << j.dump());
            session.Send("/app/ubisam", data);

            std::this_thread::sleep_for(std::chrono::seconds(1));
        } });
}
