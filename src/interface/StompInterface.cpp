#include "StompInterface.hpp"

// Core와 Interface 연결
StompInterface::StompInterface(
    const std::string &url,
    Handlers handlers)
    : handlers(std::move(handlers)),
      core(url,
           {[this]()
            {
                connected = true;
                if (this->handlers.onConnect)
                    this->handlers.onConnect();
            },
            [this]()
            {
                connected = false;
                if (this->handlers.onDisconnect)
                    this->handlers.onDisconnect();
            },
            [this](const std::string &destination, const std::string &body)
            {
                onMessageHandler(destination, body);
            }})
{
}

// 자동 정리
StompInterface::~StompInterface()
{
    stop();
}

// 시작 명령
void StompInterface::start()
{
    stopRequested = false;
    core.start();
}

// 종료 명령
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

// 연결 확인 후 core 전송 명령
void StompInterface::pub(const std::string &destination, const std::string &body)
{
    if (!connected)
        return;
    core.pub(destination, body);
}

// 구독 목록 저장 후 Core에 구독 명령
void StompInterface::sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback)
{
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, callback});
    }
    core.sub(topic, "sub-" + std::to_string(subscriptions.size() - 1));
}

// 수신 메시지를 등록된 구독 목록에서 찾아서 해당 콜백 실행
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