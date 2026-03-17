#include "StompInterface.hpp"

// 싱글톤 생성자 - Core 핸들러 연결
StompInterface::StompInterface()
    : core(
          {[this]()
           {
               // WebSocket 연결 완료 → STOMP CONNECT 프레임 전송
               std::string host = "localhost"; // core에서 host 가져오는 방법 추가 필요
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
               parseMessage(payload); // raw payload 파싱
           }})
{
}

StompInterface::~StompInterface()
{
    stop();
}

// 핸들러 초기화
void StompInterface::init(Handlers h)
{
    handlers = std::move(h);
}

// Core에 시작 명령
void StompInterface::start(const std::string &url)
{
    stopRequested = false;
    core.start(url);
}

// Core에 종료 명령
void StompInterface::stop()
{
    if (stopRequested.exchange(true))
        return;
    core.stop();
}

// 연결 상태 확인
bool StompInterface::isConnected() const
{
    return core.isConnected();
}

// STOMP SEND 프레임 조립 후 core.send()
void StompInterface::pub(const std::string &destination, const std::string &body)
{
    if (!core.isConnected())
        return;
    core.send(buildPubFrame(destination, body));
}

// 구독 목록 저장 후 STOMP SUBSCRIBE 프레임 조립 후 core.send()
void StompInterface::sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback)
{
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, callback});
    }
    core.send(buildSubFrame(topic, "sub-" + std::to_string(subscriptions.size() - 1)));
}

// ========================
// STOMP 프레임 조립 함수들
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

// raw payload → STOMP 파싱
void StompInterface::parseMessage(const std::string &payload)
{
    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    std::string frameType = payload.substr(0, firstNewline);

    if (frameType == "CONNECTED")
        return; // STOMP 핸드셰이크 응답 → 무시

    if (frameType != "MESSAGE")
        return; // MESSAGE 프레임만 처리

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

    onMessageHandler(destination, body);
}

// 구독 라우팅
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