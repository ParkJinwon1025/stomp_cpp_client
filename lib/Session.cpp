#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

// ws:// (TLS 없음) : websocketpp::config::asio_client   → asio_client.hpp 내부 struct 이름이 asio_client라 이걸 사용
// wss:// (TLS 있음) : websocketpp::config::asio_tls_client → OpenSSL 필요, asio_tls_client.hpp include 필요
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include "Session.hpp"
#include <map>
#include <functional>
#include <atomic>
#include <thread>
#include <iostream>
#include <algorithm>

typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

// =============================================
// Impl — url, host를 제외한 나머지 멤버
// =============================================
struct Session::Impl
{
    // utility_client 방식 — 클라이언트를 멤버로 보유, 생성자에서 한 번만 초기화
    // WebSocket 클라이언트 객체
    ws_client client;

    // 현재 연결의 식별자(핸들)
    websocketpp::connection_hdl hdl;

    // client와 hdl에 여러 스레드가 동시에 접근하지 못하도록 막는 뮤텍스
    mutable std::mutex clientMutex;

    // WebSocket 이벤트 루프 전용 스레드
    std::thread wsThread;

    // STOMOP CONNECTED 프레임을 받았는지 여부
    std::atomic<bool> stompReady{false};

    // 구독 ID를 만들 때 쓰는 카운터
    std::atomic<int> subCounter{0};
};

// =============================================
// Public
// =============================================

// ── 생성자 ───────────────────────────────────
// utility_client 방식 — 생성자에서 클라이언트 초기화 + perpetual 시작 + 스레드 실행
Session::Session(const std::string &u) : impl_(std::make_unique<Impl>()) // impl_(std::make_....) => 생성자(Impl 구조체를 힙에 할당해서 impl_ 포인터에 넣음.)
{
    url = u;

    auto pos = url.find("://"); // :// 위치 찾기
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos); // localhost 추출
    }

    impl_->client.clear_access_channels(websocketpp::log::alevel::all);
    impl_->client.clear_error_channels(websocketpp::log::elevel::all);
    impl_->client.init_asio(); // 네트워크 엔진(asio) 초기화

    // run()이 연결 없어도 종료되지 않도록 유지 (재연결 가능하게)
    // => client.connect()가 이벤트 루프(run())가 살아있어야 처리됨.
    impl_->client.start_perpetual();

    // client.run()을 별도 스레드에서 실행
    // 스레드가 네트워크 이벤트(연결, 수신, 종료)를 계속 처리
    // 메인 스레드는 블로킹 없이 다른 일을 할 수 있음.
    impl_->wsThread = std::thread(&ws_client::run, &impl_->client);
}

// ── 소멸자 ───────────────────────────────────
// utility_client 방식 — 소멸자에서 perpetual 종료 + 연결 닫기 + 스레드 join
Session::~Session()
{
    // run() 함수 종료
    impl_->client.stop_perpetual();

    // 연결이 살아있으면
    if (impl_->stompReady)
    {
        websocketpp::lib::error_code ec;

        // 연결을 닫는다.
        impl_->client.close(impl_->hdl, websocketpp::close::status::going_away, "", ec);
    }

    if (impl_->wsThread.joinable())
        impl_->wsThread.join(); // wsThread가 완전히 끝날 때까지 대기
}

// ── ConnectImpl ──────────────────────────────
// utility_client 방식 — 연결 객체를 가져와 핸들러 등록 후 connect
void Session::ConnectImpl(std::function<void()> callback)
{
    LOG("[SESSION] connect -> " << url);

    websocketpp::lib::error_code ec;

    // 연결 객체 생성 시도
    // get_connection()이 실패하면 ec에 에러가 담기고 성공하면 비어있음.
    auto con = impl_->client.get_connection(url, ec);
    if (ec) // ec에 에러가 담겼으면
    {
        // 연결 실패 표시 후 함수 종료
        impl_->stompReady = false;
        return;
    }

    // 연결 성공 시 — STOMP CONNECT 프레임 전송
    con->set_open_handler([this](websocketpp::connection_hdl h)
                          {
        {
            std::lock_guard<std::mutex> lock(impl_->clientMutex);

            // 전송/종료 시 사용할 연결 핸들 저장
            impl_->hdl = h;
        }
        LOG("[SESSION] Connected -> Sending CONNECT frame");
        websocketpp::lib::error_code ec;

        // Connect 프레임 전송
        std::string frame = "CONNECT\n"
                            "accept-version:1.1,1.2\n"
                            "host:" + host + "\n"
                            "heart-beat:0,0\n\n";
        frame.push_back('\0');
        impl_->client.send(h, frame, websocketpp::frame::opcode::text, ec); });

    // 메시지 수신 시 — STOMP 프레임 종류에 따라 처리
    con->set_message_handler([this, callback](websocketpp::connection_hdl, ws_client::message_ptr msg)
                             {

        // Payload 꺼내기
        const std::string &payload = msg->get_payload();
        LOG("[CORE] raw received -> '" << payload << "'");

        // STOMP 서버가 연결을 수락했을 때
        // 0번 인덱스부터 CONNECTED로 일치하는지
        if (payload.rfind("CONNECTED", 0) == 0)
        {
            LOG("[SESSION] STOMP CONNECTED received");
            impl_->stompReady = true; // STOMP 연결 완료
            callback(); // 구독 재등록
        }
        
        // 구독 중인 토픽으로 메시지를 보냈을 때
        else if (payload.rfind("MESSAGE", 0) == 0)
        {
            // destination 헤더 파싱
            std::string destination;
            auto dest_pos = payload.find("destination:");
            if (dest_pos != std::string::npos)
            {
                dest_pos += 12; // "destination:" 길이 만큼 건너뜀.
                auto end = payload.find('\n', dest_pos);
                destination = payload.substr(dest_pos, end - dest_pos);
            }

            // 헤더와 바디 분리 (빈 줄 기준)
            std::string body;
            auto body_pos = payload.find("\n\n");
            if (body_pos != std::string::npos)
                body = payload.substr(body_pos + 2);

            // destination에 해당하는 subscriber 호출
            Subscriber *sub = nullptr;
            {
                // subscribers Map을 다른 스레드가 동시에 접근하지 못하도록 잠금.
                std::lock_guard<std::mutex> lock(impl_->clientMutex);
                auto it = subscribers_.find(destination);
                if (it != subscribers_.end()) // 등록된 Subscriber를 찾지 못했을 때
                    sub = it->second;
            }
            // Subscriber를 찾았을 때
            if (sub)
                sub->HandleReceived(*this, nlohmann::json::parse(body));
        } });

    // 연결 종료 시
    con->set_close_handler([this](websocketpp::connection_hdl)
                           {
        LOG("[SESSION] Disconnected");
        impl_->stompReady = false; });

    // 연결 실패 시
    con->set_fail_handler([this](websocketpp::connection_hdl)
                          {
        LOG("[SESSION] Failed");
        impl_->stompReady = false; });

    // 실제 연결 시작
    // get_handle : 연결 객체에서 나중에 사용할 식별자를 꺼내서 멤버 변수에 저장
    impl_->hdl = con->get_handle();
    impl_->client.connect(con);
}

// ── 문자열로 보내기 ─────────────────────────
void Session::PublishImpl(const std::string &destination, const std::string &payload)
{
    LOG("[SESSION] Send(string) -> " << destination);
    if (!IsConnected())
    {
        LOG("[SESSION] Not connected — message dropped");
        return;
    }

    // SEND 프레임 조립
    std::string frame = "SEND\n"
                        "destination:" +
                        destination + "\n"
                                      "content-type:application/json\n\n" +
                        payload;

    // STOMP 끝 프레임 표시
    frame.push_back('\0');
    std::lock_guard<std::mutex> lock(impl_->clientMutex);
    websocketpp::lib::error_code ec;

    // 전송
    impl_->client.send(impl_->hdl, frame, websocketpp::frame::opcode::text, ec);
}

// ── Subscribe ────────────────────────────────
// websocketpp : 텍스트 전송 파이프(어떻게 전달하는지만 지원)
// websocket 위에 stomp 프로토콜을 올리는 방식으로 구현

// STOMP Subscribe 프레임 구조
// SUBSCRIBE
// id:sub-0
// destination:/topic/chat?

// 제공되는 함수
// connect()
// disconnect()
// send() 정도

// STOMP 프로토콜을 이 위에 직접 구현
// ── SubscribeImpl ─────────────────────────────
void Session::SubscribeImpl(const std::string &topic, Subscriber *subscriber)
{
    LOG("[SESSION] subscribe -> " << topic);

    std::lock_guard<std::mutex> lock(impl_->clientMutex);
    if (!IsConnected())
        return; // 연결되지 않았으면 바로 종료

    // 구독 ID 생성
    std::string id = "sub-" + std::to_string(impl_->subCounter++);

    // Subscribe 프레임 조립
    std::string frame = "SUBSCRIBE\n"
                        "id:" +
                        id + "\n"
                             "destination:" +
                        topic + "\n\n";

    // STOMP 프레임 끝 표시
    frame.push_back('\0');
    websocketpp::lib::error_code ec;

    // 전송
    impl_->client.send(impl_->hdl, frame, websocketpp::frame::opcode::text, ec);
}

// ── Disconnect ───────────────────────────────
// utility_client 방식 — 연결 닫기만 담당, 스레드 정리는 소멸자에서
void Session::Disconnect()
{
    LOG("[SESSION] disconnect");
    websocketpp::lib::error_code ec;
    impl_->client.close(impl_->hdl, websocketpp::close::status::going_away, "", ec); // WebSocket 연결 닫기
}

// ── IsConnected ──────────────────────────────
bool Session::IsConnected() const
{
    // STOMP CONNECTED 프레임을 받았을 때 true, 연결이 끊기면 false
    return impl_->stompReady;
}
