#pragma once
#include "StompCore.hpp"
#include <mutex>
#include <atomic>
#include <vector>

class StompInterface
{
public:
    struct Handlers
    {
        std::function<void()> onConnect;
        std::function<void()> onDisconnect;
    };

    StompInterface(const std::string &url, Handlers handlers = {});
    ~StompInterface();

    StompInterface(const StompInterface &) = delete;
    StompInterface &operator=(const StompInterface &) = delete;

    // Core에 시작 명령
    void start();

    // Core에 종료 명령
    void stop();

    // 연결 상태 확인
    bool isConnected() const;

    // 연결 확인 후 STOMP SEND 프레임 조립 및 전송
    void pub(const std::string &destination, const std::string &body);

    // 구독 목록 저장 후 STOMP SUBSCRIBE 프레임 조립 및 전송
    void sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback = nullptr);

private:
    Handlers handlers;
    StompCore core;

    std::atomic<bool> connected{false};
    std::atomic<bool> stopRequested{false};

    struct Subscription
    {
        std::string topic;
        std::function<void(const std::string &, const std::string &)> callback;
    };
    std::vector<Subscription> subscriptions;
    std::mutex subMutex;

    // raw payload를 STOMP 프레임으로 파싱
    void parseMessage(const std::string &payload);

    // 파싱된 메시지를 구독 목록에서 찾아 콜백 실행
    void routeMessage(const std::string &destination, const std::string &body);
};