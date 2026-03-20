#pragma once
#include "../core/StompCore.hpp"
#include "Subscriber.hpp"
#include <nlohmann/json.hpp>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <iostream>

// 멀티스레드 환경에서 안전하게 콘솔 출력하기 위한 뮤텍스
inline std::mutex coutMutex;
#define LOG(msg)                                           \
    {                                                      \
        std::lock_guard<std::mutex> _log_lock(coutMutex); \
        std::cout << msg << std::endl;                     \
    }

class Publisher;

// WebSocket + STOMP 연결을 관리하는 클래스
class Session
{
public:
    Session();
    ~Session();

    void Init(const std::string &url); // 서버 URL 설정
    void Connect();                    // WebSocket 연결 시작
    void Disconnect();                 // 연결 종료
    bool IsConnected() const;          // 현재 연결 상태 반환

    void Send(const std::string &destination, const std::string &body);    // 문자열 → {"payload":"..."} 래핑 후 전송
    void SendRaw(const std::string &destination, const std::string &json); // JSON 그대로 전송 (래핑 없음)

    template <typename T>
    void Send(const std::string &destination, const T &data) // 구조체 → JSON 자동 직렬화 → STOMP 프레임 전송
    {
        nlohmann::json j = data;
        SendRaw(destination, j.dump());
    }

    void Publish(Publisher *publisher); // publisher->run() 호출
    void Subscribe(const std::string &topic, Subscriber *subscriber);   // 토픽 구독 등록

private:
    StompCore core;
    std::string url;
    std::string host;

    std::atomic<bool> stopRequested{false};
    std::atomic<bool> stompReady{false};

    asio::io_service *ioService{nullptr};

    struct Subscription
    {
        std::string topic;
        Subscriber *subscriber;
    };

    std::vector<Subscription> subscriptions;
    std::mutex subMutex;

    std::vector<std::pair<std::string, std::string>> pendingMessages; // stompReady 전 Send 큐
    std::mutex pendingMutex;

    void Post(std::function<void()> task);
    void ReRegisterSubscriptions();

    std::string BuildConnectFrame() const;
    std::string BuildSendFrame(const std::string &dest, const std::string &body) const;
    std::string BuildSubscribeFrame(const std::string &topic, const std::string &id) const;

    void ParseMessage(const std::string &payload);
    void RouteMessage(const std::string &destination, const std::string &body);
};
