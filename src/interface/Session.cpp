#include "Session.hpp"
#include "Publisher.hpp"

Session::Session()
    : core(
          {// 연결 성공 시: CONNECT 프레임 전송 → 구독 재등록 → 콜백 호출
           [this](asio::io_service &ios)
           {
               ioService = &ios;

               LOG("[SESSION] Connected -> Sending CONNECT frame");
               core.Pub(BuildConnectFrame());

               ReRegisterSubscriptions();
           },
           // 연결 끊김 시: ioService, stompReady 초기화
           [this]()
           {
               LOG("[SESSION] Disconnected");
               ioService = nullptr;
               stompReady = false;
           },
           // 메시지 수신 시: 파싱 후 라우팅
           [this](const std::string &payload)
           {
               ParseMessage(payload);
           }})
{
}

Session::~Session()
{
    Disconnect();
}

void Session::Init(const std::string &u)
{
    url = u;

    auto pos = url.find("://");
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos);
    }
}

void Session::Connect()
{
    stopRequested = false;
    LOG("[SESSION] connect -> " << url);
    core.Start(url);
}

void Session::Disconnect()
{
    if (stopRequested.exchange(true))
        return;

    LOG("[SESSION] disconnect");
    core.End();
}

bool Session::IsConnected() const
{
    return core.IsConnected() && stompReady;
}

void Session::Send(const std::string &destination, const std::string &body)
{
    std::string json = "{\"payload\":\"" + body + "\"}";
    if (!stompReady)
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        pendingMessages.push_back({destination, json});
        LOG("[SESSION] Send queued (not ready) -> " << destination);
        return;
    }
    core.Pub(BuildSendFrame(destination, json));
}

void Session::SendRaw(const std::string &destination, const std::string &json)
{
    if (!stompReady)
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        pendingMessages.push_back({destination, json});
        LOG("[SESSION] SendRaw queued (not ready) -> " << destination);
        return;
    }
    core.Pub(BuildSendFrame(destination, json));
}

void Session::Publish(Publisher *publisher)
{
    publisher->run();
}

void Session::Subscribe(const std::string &topic, Subscriber *subscriber)
{
    LOG("[SESSION] subscribe -> " << topic);

    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, subscriber});
    }

    if (IsConnected())
    {
        std::string subId = "sub-" + std::to_string(subscriptions.size() - 1);
        Post([this, topic, subId]()
             { core.Sub(BuildSubscribeFrame(topic, subId)); });
    }
}

void Session::Post(std::function<void()> task)
{
    if (!ioService)
        return;
    ioService->post(task);
}

void Session::ReRegisterSubscriptions()
{
    std::lock_guard<std::mutex> lock(subMutex);
    for (size_t i = 0; i < subscriptions.size(); i++)
    {
        LOG("[SESSION] Re-subscribing -> " << subscriptions[i].topic);
        core.Sub(BuildSubscribeFrame(subscriptions[i].topic, "sub-" + std::to_string(i)));
    }
}

// ========================
// 프레임 빌드
// ========================

std::string Session::BuildConnectFrame() const
{
    std::string frame =
        "CONNECT\n"
        "accept-version:1.1,1.2\n"
        "host:" +
        host + "\n"
               "heart-beat:0,0\n\n";
    frame.push_back('\0');
    return frame;
}

std::string Session::BuildSendFrame(const std::string &dest, const std::string &body) const
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

std::string Session::BuildSubscribeFrame(const std::string &topic, const std::string &id) const
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

void Session::ParseMessage(const std::string &payload)
{
    if (payload.empty())
        return;

    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    std::string frameType = payload.substr(0, firstNewline);

    if (frameType == "CONNECTED")
    {
        LOG("[SESSION] STOMP CONNECTED received");
        stompReady = true;

        std::vector<std::pair<std::string, std::string>> pending;
        {
            std::lock_guard<std::mutex> lock(pendingMutex);
            pending = std::move(pendingMessages);
        }
        for (const auto &m : pending)
        {
            LOG("[SESSION] Flushing queued Send -> " << m.first);
            core.Pub(BuildSendFrame(m.first, m.second));
        }
        return;
    }

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

    LOG("[SESSION] MESSAGE received -> " << destination);

    Post([this, destination, body]()
         { RouteMessage(destination, body); });
}

void Session::RouteMessage(const std::string &destination, const std::string &body)
{
    std::lock_guard<std::mutex> lock(subMutex);
    for (const auto &s : subscriptions)
    {
        if (s.topic == destination && s.subscriber)
        {
            s.subscriber->dispatch(body, *this);
            break;
        }
    }
}
