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

    StompCore(const std::string &url, Handlers handlers);
    ~StompCore();

    StompCore(const StompCore &) = delete;
    StompCore &operator=(const StompCore &) = delete;

    // WebSocket 연결 시작
    void start();

    // WebSocket 연결 종료
    void stop();

    // STOMP SEND 프레임 조립 및 전송
    void pub(const std::string &destination, const std::string &body);

    // STOMP SUBSCRIBE 프레임 조립 및 전송
    void sub(const std::string &topic, const std::string &subId);

    // 연결 상태 확인
    bool isConnected() const;

private:
    std::string uri;
    std::string host;
    Handlers handlers;

    ws_client *currentClient{nullptr};
    websocketpp::connection_hdl hdl;
    mutable std::mutex clientMutex; // isConnected()에서 const이므로 mutable
    std::atomic<bool> stopRequested{false};
    std::thread wsThread;

    // 실제 WebSocket 연결 및 이벤트 루프 실행
    void tryConnect();

    // STOMP 핸드셰이크 프레임 전송
    void sendConnectFrame(ws_client &c);

    // raw payload를 STOMP 프레임으로 파싱 후 콜백 호출
    void parseMessage(const std::string &payload);

    // URL에서 호스트명 추출
    std::string parseHost(const std::string &url) const;
};