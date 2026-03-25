#include "Subscriber.hpp"
#include "Session.hpp"
#include <iostream>

void Subscriber::HandleReceived(Session &session, const nlohmann::json &json)
{
    Run(session, json);
}

void Subscriber::Run(Session &session, const nlohmann::json &json)
{
    LOG("[RECEIVED] " << json.dump());
}
