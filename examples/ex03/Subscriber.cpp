#include "Subscriber.hpp"
#include "Session.hpp"

void Subscriber::HandleReceived(Session &session, const nlohmann::json &json)
{

    Run(session, json);
}

void Subscriber::Run(Session &session, const nlohmann::json &json)
{
    std::string action = json["payload"]["action"];
    nlohmann::json j;

    if (action == "move")
    {
        j["status"] = "moving";
    }
    else if (action == "stop")
    {
        j["status"] = "stopped";
    }
    else
    {
        j["status"] = "unknown action";
    }

    session.Publish("/app/ubisam", j);
}