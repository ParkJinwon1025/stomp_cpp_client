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

    // Core에 연결 상태 확인 위임
    bool isConnected() const;

    // 연결 확인 후 Core에 전송 명령
    void pub(const std::string &destination, const std::string &body);

    // 구독 목록 저장 후 Core에 구독 명령
    void sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback = nullptr);

private:
    Handlers handlers;
    StompCore core;

    std::atomic<bool> stopRequested{false};

    struct Subscription
    {
        std::string topic;
        std::function<void(const std::string &, const std::string &)> callback;
    };
    std::vector<Subscription> subscriptions;
    std::mutex subMutex;

    // 수신 메시지를 구독 목록에서 찾아 콜백 실행
    void onMessageHandler(const std::string &destination, const std::string &body);
};