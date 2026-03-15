#include "StompInterface.hpp"

// Core와 Interface 연결
StompInterface::StompInterface(const std::string &url, Handlers handlers)
    : handlers(std::move(handlers)),
      core(url,
           {[this]()
            {
                connected = true; // 연결 시 상태 변경
                if (this->handlers.onConnect)
                    this->handlers.onConnect();
            },
            [this]()
            {
                connected = false; // 해제 시 상태 변경
                if (this->handlers.onDisconnect)
                    this->handlers.onDisconnect();
            },
            [this](const std::string &payload)
            {
                parseMessage(payload); // raw payload → Interface가 파싱
            }})
{
}

// 자동 정리
StompInterface::~StompInterface()
{
    stop();
}

// Core에 시작 명령
void StompInterface::start()
{
    stopRequested = false;
    core.start();
}

// Core에 종료 명령
void StompInterface::stop()
{
    if (stopRequested.exchange(true))
        return;
    connected = false;
    core.stop();
}

// 연결 상태 확인
bool StompInterface::isConnected() const
{
    return connected.load();
}

// 연결 확인 후 STOMP SEND 프레임 조립 및 전송
void StompInterface::pub(const std::string &destination, const std::string &body)
{
    if (!connected)
        return;

    // STOMP SEND 프레임 조립
    std::string frame =
        "SEND\n"
        "destination:" +
        destination + "\n"
                      "content-type:application/json\n\n" +
        body;
    frame.push_back('\0');

    core.sendRaw(frame);
}

// 구독 목록 저장 후 STOMP SUBSCRIBE 프레임 조립 및 전송
void StompInterface::sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback)
{
    std::string subId;
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, callback});
        subId = "sub-" + std::to_string(subscriptions.size() - 1);
    }

    // STOMP SUBSCRIBE 프레임 조립
    std::string frame =
        "SUBSCRIBE\n"
        "id:" +
        subId + "\n"
                "destination:" +
        topic + "\n\n";
    frame.push_back('\0');

    core.sendRaw(frame);
}

// raw payload를 STOMP 프레임으로 파싱
void StompInterface::parseMessage(const std::string &payload)
{
    // frameType 추출
    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    std::string frameType = payload.substr(0, firstNewline);

    if (frameType == "CONNECTED")
        return; // 핸드셰이크 응답 → 무시

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

    routeMessage(destination, body);
}

// 파싱된 메시지를 구독 목록에서 찾아 콜백 실행
void StompInterface::routeMessage(const std::string &destination, const std::string &body)
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