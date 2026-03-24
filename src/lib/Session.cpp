#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

// ws:// (TLS 없음) : websocketpp::config::asio_client   → asio_client.hpp 내부 struct 이름이 asio_client라 이걸 사용
// wss:// (TLS 있음) : websocketpp::config::asio_tls_client → OpenSSL 필요, asio_tls_client.hpp include 필요
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include "Session.hpp"
#include "Publisher.hpp"
#include "Subscriber.hpp"
#include <map>
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
    ws_client client;
    websocketpp::connection_hdl hdl;
    mutable std::mutex clientMutex;
    std::thread wsThread;

    std::atomic<bool> stompReady{false};
    std::atomic<int> subCounter{0};

    std::map<std::string, Publisher *> publishers;
    std::map<std::string, Subscriber *> subscribers;
};

// =============================================
// Public
// =============================================

// ── 생성자 ───────────────────────────────────
// utility_client 방식 — 생성자에서 클라이언트 초기화 + perpetual 시작 + 스레드 실행
Session::Session(const std::string &u) : impl_(std::make_unique<Impl>())
{
    url = u;

    auto pos = url.find("://");
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos);
    }

    impl_->client.clear_access_channels(websocketpp::log::alevel::all);
    impl_->client.clear_error_channels(websocketpp::log::elevel::all);
    impl_->client.init_asio();
    impl_->client.start_perpetual();

    impl_->wsThread = std::thread(&ws_client::run, &impl_->client);
}

// ── 소멸자 ───────────────────────────────────
// utility_client 방식 — 소멸자에서 perpetual 종료 + 연결 닫기 + 스레드 join
Session::~Session()
{
    impl_->client.stop_perpetual();

    if (impl_->stompReady)
    {
        websocketpp::lib::error_code ec;
        impl_->client.close(impl_->hdl, websocketpp::close::status::going_away, "", ec);
    }

    if (impl_->wsThread.joinable())
        impl_->wsThread.join();
}

// ── Connect ──────────────────────────────────
// utility_client 방식 — 연결 객체를 가져와 핸들러 등록 후 connect
void Session::Connect()
{
    LOG("[SESSION] connect -> " << url);

    websocketpp::lib::error_code ec;

    // 연결 객체 생성
    auto con = impl_->client.get_connection(url, ec);
    if (ec)
    {
        impl_->stompReady = false;
        return;
    }

    // 연결 성공 시 — STOMP CONNECT 프레임 전송
    con->set_open_handler([this](websocketpp::connection_hdl h)
                          {
        {
            std::lock_guard<std::mutex> lock(impl_->clientMutex);
            impl_->hdl = h;
        }
        LOG("[SESSION] Connected -> Sending CONNECT frame");
        websocketpp::lib::error_code ec;
        impl_->client.send(h, BuildConnectFrame(host), websocketpp::frame::opcode::text, ec); });

    // 메시지 수신 시 — STOMP 프레임 종류에 따라 처리
    con->set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                             {
        const std::string &payload = msg->get_payload();
        LOG("[CORE] raw received -> '" << payload << "'");
        if (payload.rfind("CONNECTED", 0) == 0)
        {
            LOG("[SESSION] STOMP CONNECTED received");
            impl_->stompReady = true;
        }
        else if (payload.rfind("MESSAGE", 0) == 0)
        {
            // destination 헤더 파싱
            std::string destination;
            auto dest_pos = payload.find("destination:");
            if (dest_pos != std::string::npos)
            {
                dest_pos += 12;
                auto end = payload.find('\n', dest_pos);
                destination = payload.substr(dest_pos, end - dest_pos);
            }

            // 헤더와 바디 분리 (빈 줄 기준)
            std::string body;
            auto body_pos = payload.find("\n\n");
            if (body_pos != std::string::npos)
                body = payload.substr(body_pos + 2);

            // destination에 해당하는 subscriber 호출
            std::lock_guard<std::mutex> lock(impl_->clientMutex);
            auto it = impl_->subscribers.find(destination);
            if (it != impl_->subscribers.end())
                it->second->dispatch(body, *this);
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

// ── 1. Send (string) ─────────────────────────
void Session::Send(const std::string &destination, const std::string payload)
{
    LOG("[SESSION] Send(string) -> " << destination);
    if (!IsConnected())
    {
        LOG("[SESSION] Not connected — message dropped");
        return;
    }
    std::string frame = "SEND\n"
                        "destination:" +
                        destination + "\n"
                                      "content-type:application/json\n\n" +
                        payload;
    std::lock_guard<std::mutex> lock(impl_->clientMutex);
    websocketpp::lib::error_code ec;
    impl_->client.send(impl_->hdl, frame, websocketpp::frame::opcode::text, ec);
}

// ── Disconnect ───────────────────────────────
// utility_client 방식 — 연결 닫기만 담당, 스레드 정리는 소멸자에서
void Session::Disconnect()
{
    LOG("[SESSION] disconnect");
    websocketpp::lib::error_code ec;
    impl_->client.close(impl_->hdl, websocketpp::close::status::going_away, "", ec);
}

// ── IsConnected ──────────────────────────────
bool Session::IsConnected() const
{
    return impl_->stompReady;
}

// ── Publish ──────────────────────────────────
void Session::Publish(const std::string &name, Publisher *publisher)
{
    if (impl_->publishers.count(name))
    {
        LOG("[SESSION] Publisher already exists: " << name);
        return;
    }
    impl_->publishers[name] = publisher;
    LOG("[SESSION] Publisher started: " << name);
    publisher->HandleStarted(*this);
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
void Session::Subscribe(const std::string &topic, Subscriber *subscriber)
{
    LOG("[SESSION] subscribe -> " << topic);

    // 등록할 Subscriber를 Map(어느 토픽에 어느 SubScriber를 연결할 지)에 저장
    std::lock_guard<std::mutex> lock(impl_->clientMutex);
    if (!impl_->stompReady)
        return;
    impl_->subscribers[topic] = subscriber;
    std::string id = "sub-" + std::to_string(impl_->subCounter++);
    websocketpp::lib::error_code ec;
    impl_->client.send(impl_->hdl, BuildSubscribeFrame(topic, id), websocketpp::frame::opcode::text, ec);
}

// =============================================
// Private
// =============================================

// ── BuildConnectFrame ────────────────────────
static std::string BuildConnectFrame(const std::string &host)
{
    std::string frame =
        "CONNECT\n"
        "accept-version:1.1,1.2\n"
        "host:" +
        host + "\n"
               "heart-beat:0,0\n\n";
    frame.push_back('\0');
    return frame;
}

// ── BuildSubscribeFrame ──────────────────────
static std::string BuildSubscribeFrame(const std::string &topic, const std::string &id)
{
    std::string frame =
        "SUBSCRIBE\n"
        "id:" +
        id + "\n"
             "destination:" +
        topic + "\n\n";
    frame.push_back('\0');
    return frame;
}
