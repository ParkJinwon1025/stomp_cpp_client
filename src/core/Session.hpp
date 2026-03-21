#pragma once

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include "Subscriber.hpp"
#include <nlohmann/json.hpp>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <string>
#include <iostream>
#include <thread>
#include <deque>

// ws:// (TLS 없음) : websocketpp::config::asio_client   → asio_client.hpp 내부 struct 이름이 asio_client라 이걸 사용
// wss:// (TLS 있음) : websocketpp::config::asio_tls_client → OpenSSL 필요, asio_tls_client.hpp include 필요
typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

// 멀티스레드 환경에서 안전하게 콘솔 출력하기 위한 뮤텍스
inline std::mutex coutMutex;
#define LOG(msg)                                          \
    {                                                     \
        std::lock_guard<std::mutex> _log_lock(coutMutex); \
        std::cout << msg << std::endl;                    \
    }

class Publisher;

class Session
{
public:
    Session(const std::string &url);
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    void Connect();
    void Disconnect();
    bool IsConnected() const;

    // Send Version 2
    // 1안 : JSON 오브젝트를 매개변수로 받기
    void Send(const std::string &destination, const nlohmann::json &j);

    // 2안 : 구조체(Struct)를 매개변수로 받기
    // // NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE 매크로가 정의된 struct만 사용 가능
    template <typename T>
    void Send(const std::string &destination, const T &data)
    {
        nlohmann::json j = data;
        SendRaw(destination, j.dump());
    }

    void Publish(Publisher *publisher);
    void Subscribe(const std::string &topic, Subscriber *subscriber);

private:
    // WebSocket
    std::string uri;
    std::string host;
    ws_client *currentClient{nullptr};
    websocketpp::connection_hdl hdl;
    asio::io_service *ioService{nullptr};
    mutable std::mutex clientMutex;
    std::thread wsThread;

    // Session
    std::string url;
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> stompReady{false};

    struct Subscription
    {
        std::string topic;
        Subscriber *subscriber;
    };
    std::vector<Subscription> subscriptions;
    std::mutex subMutex;

    // 발신 큐 — Pub/Sub 무조건 여기 거침
    std::deque<std::string> outQueue;
    std::mutex queueMutex;
    std::thread queueWorker;
    std::atomic<bool> queueStop{false};

    // Send Version 1
    void SendRaw(const std::string &destination, const std::string &body); // 문자열 → 전송 (내부용)
    void TryConnect();
    void Pub(const std::string &rawFrame);
    void Sub(const std::string &rawFrame);
    void Post(std::function<void()> task);
    void ReRegisterSubscriptions();

    std::string BuildConnectFrame() const;
    std::string BuildSendFrame(const std::string &dest, const std::string &body) const;
    std::string BuildSubscribeFrame(const std::string &topic, const std::string &id) const;
    void ParseMessage(const std::string &payload);
    void RouteMessage(const std::string &destination, const std::string &body);
};
