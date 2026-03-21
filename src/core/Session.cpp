#include "Session.hpp"
#include "Publisher.hpp"
#include <iostream>
#include <algorithm>

// =============================================
// Public
// =============================================

// 생성자 — URL 파싱 및 host 추출
Session::Session(const std::string &u)
{
    url = u;
    uri = u;

    auto pos = url.find("://");
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos);
    }
}

// 소멸자 — 연결 종료
Session::~Session()
{
    Disconnect();
}

// WebSocket 연결 시작 + 워커 스레드 시작
void Session::Connect()
{
    stopRequested = false;
    queueStop = false;
    LOG("[SESSION] connect -> " << url);

    if (wsThread.joinable())
        wsThread.join();

    wsThread = std::thread([this]()
                           { TryConnect(); });

    // 워커 스레드 — 연결 상태일 때 큐에서 꺼내서 전송
    queueWorker = std::thread([this]()
    {
        while (!queueStop)
        {
            if (stompReady)
            {
                std::string frame;
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    if (!outQueue.empty())
                    {
                        frame = outQueue.front();
                        outQueue.pop_front();
                    }
                }
                if (!frame.empty())
                    Pub(frame);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

// 연결 종료 + 워커 스레드 정지
void Session::Disconnect()
{
    if (stopRequested.exchange(true))
        return;

    LOG("[SESSION] disconnect");
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        if (currentClient)
            currentClient->stop();
    }

    queueStop = true;
    if (queueWorker.joinable())
        queueWorker.join();

    if (wsThread.joinable())
        wsThread.detach();
}

// 현재 STOMP 연결 상태 반환
bool Session::IsConnected() const
{
    std::lock_guard<std::mutex> lock(clientMutex);
    return currentClient != nullptr && stompReady;
}

// JSON 오브젝트를 문자열로 변환 후 큐에 push
void Session::Send(const std::string &destination, const nlohmann::json &j)
{
    // j.dump() : nlohmann 객체를 문자열로 변환
    SendRaw(destination, j.dump());
}

// Publisher의 HandleStarted 호출
void Session::Publish(Publisher *publisher)
{
    publisher->HandleStarted(*this);
}

// 구독 목록에 추가 + 구독 프레임 큐에 push
void Session::Subscribe(const std::string &topic, Subscriber *subscriber)
{
    LOG("[SESSION] subscribe -> " << topic);
    size_t subId;
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, subscriber});
        subId = subscriptions.size() - 1;
    }

    // 무조건 큐에 push
    std::lock_guard<std::mutex> lock(queueMutex);
    outQueue.push_back(BuildSubscribeFrame(topic, "sub-" + std::to_string(subId)));
}

// =============================================
// Private
// =============================================

// 전송 프레임 생성 후 큐에 push (미연결 시 버림)
void Session::SendRaw(const std::string &destination, const std::string &body)
{
    if (!stompReady)
    {
        LOG("[SESSION] Not connected — message dropped");
        return;
    }
    std::lock_guard<std::mutex> lock(queueMutex);
    outQueue.push_back(BuildSendFrame(destination, body));
    LOG("[SESSION] Send enqueued -> " << destination);
}

// WebSocket으로 실제 전송
void Session::Pub(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;
    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

// WebSocket으로 실제 전송 (구독용)
void Session::Sub(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;
    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

// io_service에 작업 등록
void Session::Post(std::function<void()> task)
{
    if (!ioService)
        return;
    ioService->post(task);
}

// 재연결 시 구독 목록의 모든 구독을 큐에 다시 push (중복 방지: 기존 구독 프레임 먼저 제거)
void Session::ReRegisterSubscriptions()
{
    std::lock_guard<std::mutex> lock(subMutex);

    // 큐에서 구독 프레임만 제거
    {
        std::lock_guard<std::mutex> qLock(queueMutex);
        outQueue.erase(
            std::remove_if(outQueue.begin(), outQueue.end(),
                [](const std::string &frame) { return frame.rfind("SUBSCRIBE", 0) == 0; }),
            outQueue.end());
    }

    // 구독 목록 다시 push
    for (size_t i = 0; i < subscriptions.size(); i++)
    {
        LOG("[SESSION] Re-subscribing -> " << subscriptions[i].topic);
        std::lock_guard<std::mutex> qLock(queueMutex);
        outQueue.push_back(BuildSubscribeFrame(subscriptions[i].topic, "sub-" + std::to_string(i)));
    }
}

// WebSocket 연결 시도 및 핸들러 등록
void Session::TryConnect()
{
    ws_client c;
    c.clear_access_channels(websocketpp::log::alevel::all);
    c.clear_error_channels(websocketpp::log::elevel::all);
    c.init_asio();

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
        ReRegisterSubscriptions(); });

    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
        LOG("[CORE] raw received -> '" << msg->get_payload() << "'");
        ParseMessage(msg->get_payload()); });

    c.set_close_handler([this](websocketpp::connection_hdl)
                        {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr;
            ioService = nullptr;
        }
        LOG("[SESSION] Disconnected");
        stompReady = false; });

    c.set_fail_handler([this](websocketpp::connection_hdl)
                       {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr;
            ioService = nullptr;
        }
        LOG("[SESSION] Disconnected");
        stompReady = false; });

    websocketpp::lib::error_code ec;
    auto con = c.get_connection(uri, ec);
    if (ec)
    {
        stompReady = false;
        return;
    }

    c.connect(con);
    c.run();

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        currentClient = nullptr;
    }
}

// STOMP CONNECT 프레임 생성
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

// STOMP SEND 프레임 생성
std::string Session::BuildSendFrame(const std::string &dest, const std::string &body) const
{
    std::string frame =
        "SEND\n"
        "destination:" +
        dest + "\n"
               "content-type:application/json\n\n" +
        body;
    frame.push_back('\0');
    return frame;
}

// STOMP SUBSCRIBE 프레임 생성
std::string Session::BuildSubscribeFrame(const std::string &topic, const std::string &id) const
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

// 수신된 STOMP 프레임 파싱
void Session::ParseMessage(const std::string &payload)
{
    if (payload.empty())
        return;

    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    std::string frameType = payload.substr(0, firstNewline);

    if (frameType == "CONNECTED")
    {
        LOG("[SESSION] STOMP CONNECTED received");
        stompReady = true;
        return;
    }

    if (frameType != "MESSAGE")
        return;

    std::string destination;
    auto destPos = payload.find("destination:");
    if (destPos != std::string::npos)
    {
        auto destEnd = payload.find('\n', destPos);
        destination = payload.substr(destPos + 12, destEnd - destPos - 12);
    }

    auto bodyPos = payload.find("\n\n");
    if (bodyPos == std::string::npos)
        return;

    std::string body = payload.substr(bodyPos + 2);
    if (!body.empty() && body.back() == '\0')
        body.pop_back();

    LOG("[SESSION] MESSAGE received -> " << destination);

    Post([this, destination, body]()
         { RouteMessage(destination, body); });
}

// destination에 맞는 Subscriber에게 메시지 전달
void Session::RouteMessage(const std::string &destination, const std::string &body)
{
    std::lock_guard<std::mutex> lock(subMutex);
    for (const auto &s : subscriptions)
    {
        if (s.topic == destination && s.subscriber)
        {
            s.subscriber->dispatch(body, *this);
            break;
        }
    }
}
