#include "Session.hpp"
#include <chrono>

Session::Session()
    : core(
          {// 연결 성공 시
           [this](asio::io_service &ios)
           {
               ioService = &ios;

               LOG("[SESSION] Connected -> Sending CONNECT frame");
               core.Pub(buildConnectFrame());

               reRegisterSubscriptions();
               heartbeat_Start();

               // 등록된 콜백 전부 호출 (Publisher.flush 등)
               for (auto &cb : onConnectedCallbacks)
                   cb();
           },
           // 연결 끊김 시
           [this]()
           {
               LOG("[SESSION] Disconnected");
               heartbeat_Cancel();
               ioService = nullptr;
           },
           // 메시지 수신 시
           [this](const std::string &payload)
           {
               parseMessage(payload);
           },
           // Pong 수신 시
           [this]()
           {
               LOG("[HEARTBEAT] Pong received");
           },
           // Pong 타임아웃 시
           [this]()
           {
               LOG("[HEARTBEAT] Pong timeout");
           }})
{
}

Session::~Session()
{
    disconnect();
}

void Session::init(const std::string &u)
{
    url = u;

    // host 추출
    auto pos = url.find("://");
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos);
    }
}

void Session::connect()
{
    stopRequested = false;
    LOG("[SESSION] connect -> " << url);
    core.Start(url);
}

void Session::disconnect()
{
    if (stopRequested.exchange(true))
        return;

    LOG("[SESSION] disconnect");
    heartbeat_Cancel();
    core.End();
}

bool Session::isConnected() const
{
    return core.IsConnected();
}

void Session::publish(const std::string &destination, const std::string &body)
{
    // ASIO 스레드로 전송 작업 위임
    post([this, destination, body]()
         { core.Pub(buildSendFrame(destination, body)); });
}

void Session::subscribe(const std::string &topic, Subscriber *subscriber)
{
    LOG("[SESSION] subscribe -> " << topic);

    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, subscriber});
    }

    // 현재 연결 중이면 바로 구독 프레임 전송
    if (isConnected())
    {
        std::string subId = "sub-" + std::to_string(subscriptions.size() - 1);
        post([this, topic, subId]()
             { core.Sub(buildSubscribeFrame(topic, subId)); });
    }
}

void Session::onConnected(std::function<void()> cb)
{
    onConnectedCallbacks.push_back(cb);
}

void Session::post(std::function<void()> task)
{
    if (!ioService)
        return;
    ioService->post(task);
}

void Session::heartbeat_Start()
{
    if (!ioService)
        return;
    heartbeatTimer = std::make_shared<asio::steady_timer>(*ioService);
    heartbeat_Schedule();
}

void Session::heartbeat_Schedule()
{
    if (!heartbeatTimer || !ioService)
        return;

    heartbeatTimer->expires_after(std::chrono::seconds(10));
    heartbeatTimer->async_wait([this](const asio::error_code &ec)
                               {
        if (ec) return;
        if (core.IsConnected())
        {
            LOG("[HEARTBEAT] Ping sent");
            core.Pub("\n");
        }
        heartbeat_Schedule(); });
}

void Session::heartbeat_Cancel()
{
    if (heartbeatTimer)
    {
        heartbeatTimer->cancel();
        heartbeatTimer.reset();
        LOG("[HEARTBEAT] Timer cancelled");
    }
}

void Session::reRegisterSubscriptions()
{
    std::lock_guard<std::mutex> lock(subMutex);
    for (size_t i = 0; i < subscriptions.size(); i++)
    {
        LOG("[SESSION] Re-subscribing -> " << subscriptions[i].topic);
        core.Sub(buildSubscribeFrame(subscriptions[i].topic, "sub-" + std::to_string(i)));
    }
}

// ========================
// 프레임 빌드
// ========================

std::string Session::buildConnectFrame() const
{
    std::string frame =
        "CONNECT\n"
        "accept-version:1.1,1.2\n"
        "heart-beat:10000,10000\n\n";
    frame.push_back('\0');
    return frame;
}

std::string Session::buildSendFrame(const std::string &dest, const std::string &body) const
{
    std::string frame =
        "SEND\n"
        "destination:" +
        dest + "\n"
               "content-type:application/json\n\n" +
        body;
    frame.push_back('\0');
    return frame;
}

std::string Session::buildSubscribeFrame(const std::string &topic, const std::string &id) const
{
    std::string frame =
        "SUBSCRIBE\n"
        "id:" +
        id + "\n"
             "destination:" +
        topic + "\n\n";
    frame.push_back('\0');
    return frame;
}

// ========================
// 수신 처리
// ========================

void Session::parseMessage(const std::string &payload)
{
    if (payload == "\n" || payload.empty())
    {
        LOG("[HEARTBEAT] Pong received");
        return;
    }

    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    std::string frameType = payload.substr(0, firstNewline);

    if (frameType == "CONNECTED")
    {
        LOG("[SESSION] STOMP CONNECTED received");
        return;
    }

    if (frameType != "MESSAGE")
        return;

    // destination 추출
    std::string destination;
    auto destPos = payload.find("destination:");
    if (destPos != std::string::npos)
    {
        auto destEnd = payload.find('\n', destPos);
        destination = payload.substr(destPos + 12, destEnd - destPos - 12);
    }

    // body 추출
    auto bodyPos = payload.find("\n\n");
    if (bodyPos == std::string::npos)
        return;

    std::string body = payload.substr(bodyPos + 2);
    if (!body.empty() && body.back() == '\0')
        body.pop_back();

    LOG("[SESSION] MESSAGE received -> " << destination);

    // recvQueue 없이 바로 Post로 콜백 호출
    post([this, destination, body]()
         { routeMessage(destination, body); });
}

void Session::routeMessage(const std::string &destination, const std::string &body)
{
    std::lock_guard<std::mutex> lock(subMutex);
    for (const auto &s : subscriptions)
    {
        if (s.topic == destination && s.subscriber)
        {
            s.subscriber->received(body, *this);
            break;
        }
    }
}