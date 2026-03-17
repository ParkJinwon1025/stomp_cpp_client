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
        std::function<void(const std::string &)> onRawMessage; // raw payload만 위로 올림
    };

    StompCore(Handlers handlers);
    ~StompCore();

    StompCore(const StompCore &) = delete;
    StompCore &operator=(const StompCore &) = delete;

    // WebSocket 연결 시작
    void start(const std::string &url);

    // WebSocket 연결 종료
    void stop();

    // raw string 전송만 담당
    void send(const std::string &rawFrame);

    // 연결 상태 확인
    bool isConnected() const;

private:
    std::string uri;
    std::string host;
    Handlers handlers;

    ws_client *currentClient{nullptr};
    websocketpp::connection_hdl hdl;
    mutable std::mutex clientMutex;
    std::atomic<bool> stopRequested{false};
    std::thread wsThread;

    // 실제 WebSocket 연결 및 이벤트 루프 실행
    void tryConnect();

    // URL에서 호스트명 추출 (start 내부용)
    std::string parseHost(const std::string &url) const;
};