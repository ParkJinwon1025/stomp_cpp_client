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
        std::function<void()> onConnect;
        std::function<void()> onDisconnect;
        std::function<void(const std::string &, const std::string &)> onMessage;
    };

    StompCore(
        const std::string &url,
        Handlers handlers);

    ~StompCore();

    StompCore(const StompCore &) = delete;
    StompCore &operator=(const StompCore &) = delete;

    void start();
    void stop();

    void pub(const std::string &destination, const std::string &body);
    void sub(const std::string &topic, const std::string &subId);

private:
    std::string uri;
    std::string host;
    Handlers handlers;

    ws_client *currentClient{nullptr};
    websocketpp::connection_hdl hdl;
    std::mutex clientMutex;
    std::atomic<bool> stopRequested{false};
    std::thread wsThread;

    void tryConnect();
    void sendConnectFrame(ws_client &c);
    void parseMessage(const std::string &payload);
    std::string parseHost(const std::string &url) const;
};