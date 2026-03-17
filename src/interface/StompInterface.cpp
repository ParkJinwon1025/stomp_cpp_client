#include "StompInterface.hpp"

StompInterface::StompInterface()
    : core(
          {[this]()
           {
               core.Pub(Frame_BuildConnect(host));
               if (handlers.onConnect)
                   handlers.onConnect();
           },
           [this]()
           {
               if (handlers.onDisconnect)
                   handlers.onDisconnect();
           },
           [this](const std::string &payload)
           {
               Message_Parse(payload);
           }})
{
}

StompInterface::~StompInterface()
{
    Connection_End();
}

void StompInterface::init(Handlers h)
{
    handlers = std::move(h);
}

void StompInterface::Connection_Start(const std::string &url)
{
    auto pos = url.find("://");
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos);
    }

    stopRequested = false;
    core.Start(url);
}

void StompInterface::Connection_End()
{
    if (stopRequested.exchange(true))
        return;
    core.End();
}

bool StompInterface::Connection_IsAlive() const
{
    return core.IsConnected();
}

void StompInterface::Message_Publish(const std::string &destination, const std::string &body)
{
    if (!core.IsConnected())
        return;
    core.Pub(Frame_BuildPub(destination, body));
}

void StompInterface::Message_Subscribe(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback)
{
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, callback});
    }
    core.Sub(Frame_BuildSub(topic, "sub-" + std::to_string(subscriptions.size() - 1)));
}

// ========================
// STOMP 프레임 조립
// ========================

std::string StompInterface::Frame_BuildConnect(const std::string &host) const
{
    std::string frame = "CONNECT\naccept-version:1.2\nhost:" + host + "\n\n";
    frame.push_back('\0');
    return frame;
}

std::string StompInterface::Frame_BuildPub(const std::string &destination, const std::string &body) const
{
    std::string frame =
        "SEND\n"
        "destination:" +
        destination + "\n"
                      "content-type:application/json\n\n" +
        body;
    frame.push_back('\0');
    return frame;
}

std::string StompInterface::Frame_BuildSub(const std::string &topic, const std::string &subId) const
{
    std::string frame =
        "SUBSCRIBE\n"
        "id:" +
        subId + "\n"
                "destination:" +
        topic + "\n\n";
    frame.push_back('\0');
    return frame;
}

// ========================
// 파싱 및 라우팅
// ========================

void StompInterface::Message_Parse(const std::string &payload)
{
    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    std::string frameType = payload.substr(0, firstNewline);

    if (frameType == "CONNECTED")
        return;

    if (frameType != "MESSAGE")
        return;

    std::string destination;
    auto destPos = payload.find("destination:");
    if (destPos != std::string::npos)
    {
        auto destEnd = payload.find('\n', destPos);
        destination = payload.substr(destPos + 12, destEnd - destPos - 12);
    }

    auto bodyPos = payload.find("\n\n");
    if (bodyPos == std::string::npos)
        return;

    std::string body = payload.substr(bodyPos + 2);
    if (!body.empty() && body.back() == '\0')
        body.pop_back();

    Message_Route(destination, body);
}

void StompInterface::Message_Route(const std::string &destination, const std::string &body)
{
    std::lock_guard<std::mutex> lock(subMutex);
    for (const auto &s : subscriptions)
    {
        if (s.topic == destination && s.callback)
        {
            s.callback(destination, body);
            break;
        }
    }
}