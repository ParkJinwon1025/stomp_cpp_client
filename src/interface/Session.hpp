#pragma once
#include "../core/StompCore.hpp"
#include "Subscriber.hpp"
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <iostream>

inline std::mutex coutMutex;
#define LOG(msg)                                          \
    {                                                     \
        std::lock_guard<std::mutex> _log_lock(coutMutex); \
        std::cout << msg << std::endl;                    \
    }

class Session
{
public:
    Session();
    ~Session();

    void init(const std::string &url);

    void connect();
    void disconnect();
    bool isConnected() const;

    // Publisher가 직접 전송할 때 사용
    void publish(const std::string &destination, const std::string &body);

    // Subscriber* 직접 받아서 등록
    void subscribe(const std::string &topic, Subscriber *subscriber);

    // 연결 성공 시 호출할 콜백 등록 (Publisher.flush 등)
    void onConnected(std::function<void()> cb);

private:
    StompCore core;
    std::string url;
    std::string host;
    std::atomic<bool> stopRequested{false};

    asio::io_service *ioService{nullptr};
    std::shared_ptr<asio::steady_timer> heartbeatTimer;

    struct Subscription
    {
        std::string topic;
        Subscriber *subscriber;
    };
    std::vector<Subscription> subscriptions;
    std::mutex subMutex;

    std::vector<std::function<void()>> onConnectedCallbacks;

    void post(std::function<void()> task);

    void heartbeat_Start();
    void heartbeat_Schedule();
    void heartbeat_Cancel();

    void reRegisterSubscriptions();

    // 프레임 빌드
    std::string buildConnectFrame() const;
    std::string buildSendFrame(const std::string &dest, const std::string &body) const;
    std::string buildSubscribeFrame(const std::string &topic, const std::string &id) const;

    // 수신 처리
    void parseMessage(const std::string &payload);
    void routeMessage(const std::string &destination, const std::string &body);
};