#include "Subscriber.hpp"
#include "Session.hpp"

void Subscriber::dispatch(const std::string &body, Session &session)
{
    nlohmann::json j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded())
        receive(body, session);
    else
        receive(j, session);
}

void Subscriber::receive(const std::string &body, Session &session)
{
    LOG("received: " << body);
}

void Subscriber::receive(const nlohmann::json &json, Session &session)
{
    LOG("received: " << json.dump());
}
