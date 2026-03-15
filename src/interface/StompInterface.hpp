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

    StompInterface(
        const std::string &url,
        Handlers handlers = {});

    ~StompInterface();

    StompInterface(const StompInterface &) = delete;
    StompInterface &operator=(const StompInterface &) = delete;

    // 논블로킹 → 즉시 반환
    void start();
    void stop();
    bool isConnected() const;

    void pub(const std::string &destination, const std::string &body);
    // start() 후 isConnected() 확인 후 호출 권장
    // callback 생략 시 구독만 등록
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

    void onMessageHandler(const std::string &destination, const std::string &body);
};