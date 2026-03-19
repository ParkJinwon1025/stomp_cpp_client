#pragma once

#define ASIO_STANDALONE         // ASIO를 단독으로 사용 (Boost 없이)
#define _WEBSOCKETPP_CPP11_STL_ // websocketpp C++11 STL 사용 설정

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>

// TLS 없는 WebSocket 클라이언트 타입 별칭
typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

// WebSocket + STOMP 연결의 핵심 로직을 담당하는 클래스
// Session이 이 클래스를 소유하고 사용
class StompCore
{
public:
    // 외부에서 주입받는 이벤트 핸들러 묶음
    struct Handlers
    {
        std::function<void(asio::io_service &)> onConnect;     // 연결 성공 시 호출
        std::function<void()> onDisconnect;                    // 연결 끊김 시 호출
        std::function<void(const std::string &)> onRawMessage; // 메시지 수신 시 호출
    };

    StompCore(Handlers handlers);
    ~StompCore();

    // mutex와 같은 내부에 복사 불가능한 멤버가 있음.
    // 포인터 멤버를 복사하면 위험함.
    StompCore(const StompCore &) = delete;            // 복사 금지
    StompCore &operator=(const StompCore &) = delete; // 대입 금지

    void Start(const std::string &url);    // WebSocket 연결 시작 (별도 스레드)
    void End();                            // 연결 종료
    void Pub(const std::string &rawFrame); // STOMP 프레임 전송 (SEND, CONNECT 등)
    void Sub(const std::string &rawFrame); // STOMP 구독 프레임 전송 (SUBSCRIBE)
    bool IsConnected() const;              // 현재 연결 여부 반환

private:
    std::string uri;   // 연결 대상 URL
    std::string host;  // 호스트명
    Handlers handlers; // 이벤트 핸들러

    ws_client *currentClient{nullptr};      // 현재 연결된 WebSocket 클라이언트 (null이면 미연결)
    websocketpp::connection_hdl hdl;        // 현재 연결 핸들
    asio::io_service *ioService{nullptr};   // io_service 스레드에서 send()하기 위해 저장
    mutable std::mutex clientMutex;         // currentClient 접근 보호 뮤텍스
    std::atomic<bool> stopRequested{false}; // 종료 요청 플래그
    std::thread wsThread;                   // WebSocket 이벤트 루프 스레드

    void TryConnect();                                   // 한 번 연결 시도 (wsThread에서 실행)
    std::string ParseHost(const std::string &url) const; // URL에서 호스트명 추출
};
