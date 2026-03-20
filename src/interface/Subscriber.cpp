#include "Subscriber.hpp"
#include "Session.hpp"

void Subscriber::dispatch(const std::string &body, Session &session)
{
    nlohmann::json j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded())
        receive(session); // 파싱 실패 → 문자열 → receive(session)
    else
        receive(j, session); // 파싱 성공 → JSON → receive(json, session)
}
