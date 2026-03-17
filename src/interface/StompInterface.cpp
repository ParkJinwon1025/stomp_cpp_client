#include "StompInterface.hpp"

StompInterface::StompInterface()
    : core(
          {[this]()
           {
               core.send(buildConnectFrame(host));
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
               parseMessage(payload);
           }})
{
}

StompInterface::~StompInterface()
{
    stop();
}

void StompInterface::init(Handlers h)
{
    handlers = std::move(h);
}

void StompInterface::start(const std::string &url)
{
    // url에서 host 추출 후 저장
    auto pos = url.find("://");
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos);
    }

    stopRequested = false;
    core.start(url);
}

void StompInterface::stop()
{
    if (stopRequested.exchange(true))
        return;
    core.stop();
}

bool StompInterface::isConnected() const
{
    return core.isConnected();
}

void StompInterface::pub(const std::string &destination, const std::string &body)
{
    if (!core.isConnected())
        return;
    core.send(buildPubFrame(destination, body));
}

void StompInterface::sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback)
{
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, callback});
    }
    core.send(buildSubFrame(topic, "sub-" + std::to_string(subscriptions.size() - 1)));
}

// ========================
// STOMP 프레임 조립
// ========================

std::string StompInterface::buildConnectFrame(const std::string &host) const
{
    std::string frame = "CONNECT\naccept-version:1.2\nhost:" + host + "\n\n";
    frame.push_back('\0');
    return frame;
}

std::string StompInterface::buildPubFrame(const std::string &destination, const std::string &body) const
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

std::string StompInterface::buildSubFrame(const std::string &topic, const std::string &subId) const
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

void StompInterface::parseMessage(const std::string &payload)
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

    onMessageHandler(destination, body);
}

void StompInterface::onMessageHandler(const std::string &destination, const std::string &body)
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