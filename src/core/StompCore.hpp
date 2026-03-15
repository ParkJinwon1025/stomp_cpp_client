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
        std::function<void(const std::string &)> onRawMessage; // raw payload 그대로 전달
    };

    StompCore(const std::string &url, Handlers handlers);
    ~StompCore();

    StompCore(const StompCore &) = delete;
    StompCore &operator=(const StompCore &) = delete;

    // 웹소켓 연결 시작
    void start();

    // 웹소켓 연결 종료
    void stop();

    // raw 프레임 전송
    void sendRaw(const std::string &frame);

private:
    std::string uri;
    std::string host;
    Handlers handlers;

    ws_client *currentClient{nullptr};
    websocketpp::connection_hdl hdl;
    std::mutex clientMutex;
    std::atomic<bool> stopRequested{false};
    std::thread wsThread;

    // 실제 WebSocket 연결 및 이벤트 루프 실행
    void tryConnect();

    // STOMP 핸드셰이크 프레임 전송
    void sendConnectFrame();

    // URL에서 호스트명 추출
    std::string parseHost(const std::string &url) const;
};