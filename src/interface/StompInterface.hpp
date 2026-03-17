#pragma once
#include "StompCore.hpp"
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <string>

class StompInterface
{
public:
    struct Handlers
    {
        std::function<void()> onConnect;
        std::function<void()> onDisconnect;
    };

    // 싱글톤
    // 전역 접근점
    // 연결은 항상 하나고 sub가 같은 벡터에 쌓여서 라우팅이 정확함
    // start/stop 관리 포인트가 하나
    static StompInterface &getInstance()
    {
        static StompInterface instance; // 최초 1회만 생성
        return instance;
    }

    // 2. 복사/대입 금지
    StompInterface(const StompInterface &) = delete;
    StompInterface &operator=(const StompInterface &) = delete;

    void init(Handlers handlers);

    void start(const std::string &url);
    void stop();
    bool isConnected() const;

    void pub(const std::string &destination, const std::string &body);
    void sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback = nullptr);

private:
    // 생성자 private
    StompInterface();
    ~StompInterface();

    Handlers handlers;
    StompCore core;
    std::string host;
    std::atomic<bool> stopRequested{false};

    struct Subscription
    {
        std::string topic;
        std::function<void(const std::string &, const std::string &)> callback;
    };
    std::vector<Subscription> subscriptions;
    std::mutex subMutex;

    std::string buildConnectFrame(const std::string &host) const;
    std::string buildPubFrame(const std::string &destination, const std::string &body) const;
    std::string buildSubFrame(const std::string &topic, const std::string &subId) const;

    void parseMessage(const std::string &payload);
    void onMessageHandler(const std::string &destination, const std::string &body);
};