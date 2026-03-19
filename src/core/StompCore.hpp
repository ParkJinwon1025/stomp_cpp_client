#pragma once

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>

typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

class StompCore
{
public:
    struct Handlers
    {
        std::function<void(asio::io_service &)> onConnect;
        std::function<void()> onDisconnect;
        std::function<void(const std::string &)> onRawMessage;
        std::function<void()> onPong;        // pong 수신 시
        std::function<void()> onPongTimeout; // pong 타임아웃 시
    };

    StompCore(Handlers handlers);
    ~StompCore();

    StompCore(const StompCore &) = delete;
    StompCore &operator=(const StompCore &) = delete;

    void Start(const std::string &url);
    void End();
    void Pub(const std::string &rawFrame);
    void Sub(const std::string &rawFrame);
    bool IsConnected() const;

private:
    std::string uri;
    std::string host;
    Handlers handlers;

    ws_client *currentClient{nullptr};
    websocketpp::connection_hdl hdl;
    mutable std::mutex clientMutex;
    std::atomic<bool> stopRequested{false};
    std::thread wsThread;

    void TryConnect();
    std::string ParseHost(const std::string &url) const;
};