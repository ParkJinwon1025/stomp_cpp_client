#include "Session.hpp"
#include "Publisher.hpp"
#include <iostream>
#include <algorithm>

// =============================================
// Public
// =============================================

// ── 생성자 ───────────────────────────────────
Session::Session(const std::string &u)
{

    // URL에서 host 파싱
    url = u;
    uri = u;

    auto pos = url.find("://");
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos); // CONNECT 프레임의 헤더에 사용
    }
}

// ── 소멸자 ───────────────────────────────────
Session::~Session()
{
    Disconnect();
}

// ── Connect ──────────────────────────────────
void Session::Connect()
{
    // 종료 요청 플래그 초기화
    stopRequested = false;
    LOG("[SESSION] connect -> " << url);

    // 이전에 돌던 쓰레드가 있으면 완전히 끝날 때까지 기다림
    if (wsThread.joinable())
        wsThread.join();

    // TryConnect를 백그라운드 쓰레드로 띄움
    wsThread = std::thread([this]()
                           { TryConnect(); });
}

// ── Disconnect ───────────────────────────────
void Session::Disconnect()
{
    // Disconnect 중복 실행 방지
    if (stopRequested.exchange(true))
        return;

    LOG("[SESSION] disconnect");
    {
        // WebSocket 이벤트 루프(c.run()) 종료 요청
        std::lock_guard<std::mutex> lock(clientMutex);
        if (currentClient)
            currentClient->stop();
    }

    // 쓰레드가 실행 중인지 확인
    if (wsThread.joinable())

        // 쓰레드를 메인 쓰레드와 분리
        wsThread.detach();
}

// ── IsConnected ──────────────────────────────
bool Session::IsConnected() const
{
    std::lock_guard<std::mutex> lock(clientMutex);
    return currentClient != nullptr && stompReady;
}

// ── Publish ──────────────────────────────────
void Session::Publish(Publisher *publisher)
{
    publisher->HandleStarted(*this);
}

// ── 1. Send (string) ────────────────────────────
void Session::Send(const std::string &destination, const std::string body)
{
    LOG("[SESSION] Send(string) -> " << destination);
    if (!IsConnected())
    {
        LOG("[SESSION] Not connected — message dropped");
        return;
    }
    std::string frame = BuildSendFrame(destination, body);
    Pub(frame);
}

// ──2.  BuildSendFrame ───────────────────────────
// 메시지를 보낼 때마다 사용
std::string Session::BuildSendFrame(const std::string &dest, const std::string &body) const
{
    LOG("[SESSION] BuildSendFrame -> " << dest);
    std::string frame =
        "SEND\n"
        "destination:" +
        dest + "\n"
               "content-type:application/json\n\n" +
        body;
    frame.push_back('\0');
    return frame;
}

// ──3.  Pub ──────────────────────────────────────
void Session::Pub(const std::string &rawFrame)
{
    LOG("[SESSION] Pub -> sending frame");
    std::lock_guard<std::mutex> lock(clientMutex);
    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

// ── 2. Send (json) ──────────────────────────────
void Session::Send(const std::string &destination, const nlohmann::json &j)
{
    LOG("[SESSION] Send(json)   -> " << destination);
    Send(destination, j.dump());
}

// ── 3. Send (struct) : hpp에 템플릿으로 구현 ────

// ── Subscribe ────────────────────────────────
// void Session::Subscribe(const std::string &topic, Subscriber *subscriber)
// {
//     LOG("[SESSION] subscribe -> " << topic);
//     size_t subId;
//     {
//         std::lock_guard<std::mutex> lock(subMutex);
//         subscriptions.push_back({topic, subscriber});
//         subId = subscriptions.size() - 1;
//     }
//     Sub(BuildSubscribeFrame(topic, "sub-" + std::to_string(subId)));
// }

// =============================================
// Private
// =============================================

// ── Sub ──────────────────────────────────────
// void Session::Sub(const std::string &rawFrame)
// {
//     std::lock_guard<std::mutex> lock(clientMutex);
//     if (!currentClient)
//         return;
//     websocketpp::lib::error_code ec;
//     currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
// }

// ── ReRegisterSubscriptions ──────────────────
// void Session::ReRegisterSubscriptions()
// {
//     std::lock_guard<std::mutex> lock(subMutex);
//     for (size_t i = 0; i < subscriptions.size(); i++)
//     {
//         LOG("[SESSION] Re-subscribing -> " << subscriptions[i].topic);
//         Sub(BuildSubscribeFrame(subscriptions[i].topic, "sub-" + std::to_string(i)));
//     }
// }

// ── TryConnect ───────────────────────────────
void Session::TryConnect()
{
    // WebSocket 클라이언트 생성 및 로그 비활성화
    ws_client c;
    c.clear_access_channels(websocketpp::log::alevel::all);
    c.clear_error_channels(websocketpp::log::elevel::all);
    c.init_asio();

    // 연결 성공 시 — 클라이언트 정보 저장 후 STOMP CONNECT 프레임 전송
    c.set_open_handler([this, &c](websocketpp::connection_hdl h)
                       {
                           {
                               std::lock_guard<std::mutex> lock(clientMutex);
                               hdl = h;
                               currentClient = &c;
                               ioService = &c.get_io_service();
                           }
                           LOG("[SESSION] Connected -> Sending CONNECT frame");
                           Pub(BuildConnectFrame());
                           // ReRegisterSubscriptions();
                       });

    // 메시지 수신 시 — STOMP CONNECTED 프레임이면 stompReady = true
    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
        const std::string &payload = msg->get_payload();
        LOG("[CORE] raw received -> '" << payload << "'");
        if (payload.rfind("CONNECTED", 0) == 0)
        {
            LOG("[SESSION] STOMP CONNECTED received");
            stompReady = true;
        } });

    // 연결 종료 시 — 클라이언트 정보 초기화
    c.set_close_handler([this](websocketpp::connection_hdl)
                        {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr;
            ioService = nullptr;
        }
        LOG("[SESSION] Disconnected");
        stompReady = false; });

    // 연결 실패 시 — 클라이언트 정보 초기화
    c.set_fail_handler([this](websocketpp::connection_hdl)
                       {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr;
            ioService = nullptr;
        }
        LOG("[SESSION] Disconnected");
        stompReady = false; });

    // 서버 연결 시도
    websocketpp::lib::error_code ec;
    auto con = c.get_connection(uri, ec);
    if (ec)
    {
        stompReady = false;
        return;
    }

    c.connect(con);
    c.run(); // 블로킹 — 연결이 끊길 때까지 이벤트 루프 실행

    // 루프 종료 후 클라이언트 정보 초기화
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        currentClient = nullptr;
    }
}

// ── BuildConnectFrame ────────────────────────
// 서버 연결 처음할 때 보내는 핸드셰이크
std::string Session::BuildConnectFrame() const
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
// std::string Session::BuildSubscribeFrame(const std::string &topic, const std::string &id) const
// {
//     std::string frame =
//         "SUBSCRIBE\n"
//         "id:" +
//         id + "\n"
//              "destination:" +
//         topic + "\n\n";
//     frame.push_back('\0');
//     return frame;
// }
