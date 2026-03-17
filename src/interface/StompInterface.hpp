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

    // 싱글톤 접근점
    static StompInterface &getInstance()
    {
        static StompInterface instance;
        return instance;
    }

    StompInterface(const StompInterface &) = delete;
    StompInterface &operator=(const StompInterface &) = delete;

    // 핸들러 설정 (getInstance 후 초기화용)
    void init(Handlers handlers);

    // Core에 시작 명령
    void start(const std::string &url);

    // Core에 종료 명령
    void stop();

    // 연결 상태 확인
    bool isConnected() const;

    // STOMP SEND 프레임 조립 후 전송
    void pub(const std::string &destination, const std::string &body);

    // STOMP SUBSCRIBE 프레임 조립 후 전송
    void sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback = nullptr);

private:
    StompInterface(); // 싱글톤이므로 private
    ~StompInterface();

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

    // STOMP 프레임 조립 함수들
    std::string buildConnectFrame(const std::string &host) const;
    std::string buildPubFrame(const std::string &destination, const std::string &body) const;
    std::string buildSubFrame(const std::string &topic, const std::string &subId) const;

    // raw payload → STOMP 파싱 후 라우팅
    void parseMessage(const std::string &payload);

    // 구독 라우팅
    void onMessageHandler(const std::string &destination, const std::string &body);
};