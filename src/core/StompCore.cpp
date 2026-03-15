#include "StompCore.hpp"

// 생성자
StompCore::StompCore(const std::string &url, Handlers handlers)
    : uri(url),
      host(parseHost(url)),
      handlers(std::move(handlers))
{
}

// 소멸자
StompCore::~StompCore()
{
    stop();
}

// URL에서 호스트명 추출
std::string StompCore::parseHost(const std::string &url) const
{
    auto start = url.find("://");
    if (start == std::string::npos)
        return "localhost";
    start += 3;
    auto end = url.find_first_of(":/", start);
    return url.substr(start, end - start);
}

// WebSocket 연결 시작
void StompCore::start()
{
    if (stopRequested)
        return;

    if (wsThread.joinable())
        wsThread.join();

    wsThread = std::thread([this]()
                           { tryConnect(); });
}

// WebSocket 연결 종료 및 스레드 정리
void StompCore::stop()
{
    if (stopRequested.exchange(true))
        return;

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        if (currentClient)
            currentClient->stop();
    }

    if (wsThread.joinable())
        wsThread.detach();
}

// 연결 상태 확인
bool StompCore::isConnected() const
{
    std::lock_guard<std::mutex> lock(clientMutex);
    return currentClient != nullptr;
}

// 실제 WebSocket 연결 및 이벤트 루프 실행
void StompCore::tryConnect()
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
        }
        sendConnectFrame(c); // STOMP 핸드셰이크

        if (handlers.onConnect)
            handlers.onConnect(); });

    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
                              parseMessage(msg->get_payload()); // 수신 메시지 파싱
                          });

    c.set_close_handler([this](websocketpp::connection_hdl)
                        {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr;
        }
        if (handlers.onDisconnect)
            handlers.onDisconnect(); });

    c.set_fail_handler([this](websocketpp::connection_hdl)
                       {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr;
        }
        if (handlers.onDisconnect)
            handlers.onDisconnect(); });

    websocketpp::lib::error_code ec;
    auto con = c.get_connection(uri, ec);
    if (ec)
    {
        if (handlers.onDisconnect)
            handlers.onDisconnect();
        return;
    }

    c.connect(con);
    c.run();

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        currentClient = nullptr;
    }
}

// STOMP SEND 프레임 조립 및 전송
void StompCore::pub(const std::string &destination, const std::string &body)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;

    std::string frame =
        "SEND\n"
        "destination:" +
        destination + "\n"
                      "content-type:application/json\n\n" +
        body;
    frame.push_back('\0');

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, frame, websocketpp::frame::opcode::text, ec);
}

// STOMP SUBSCRIBE 프레임 조립 및 전송
void StompCore::sub(const std::string &topic, const std::string &subId)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;

    std::string frame =
        "SUBSCRIBE\n"
        "id:" +
        subId + "\n"
                "destination:" +
        topic + "\n\n";
    frame.push_back('\0');

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, frame, websocketpp::frame::opcode::text, ec);
}

// STOMP 핸드셰이크 프레임 전송
void StompCore::sendConnectFrame(ws_client &c)
{
    std::string frame = "CONNECT\naccept-version:1.2\nhost:" + host + "\n\n";
    frame.push_back('\0');
    c.send(hdl, frame, websocketpp::frame::opcode::text);
}

// raw payload를 STOMP 프레임으로 파싱 후 콜백 호출
void StompCore::parseMessage(const std::string &payload)
{
    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    std::string frameType = payload.substr(0, firstNewline);

    if (frameType == "CONNECTED")
        return; // 핸드셰이크 응답 → 무시

    if (frameType != "MESSAGE")
        return;

    // destination 추출
    std::string destination;
    auto destPos = payload.find("destination:");
    if (destPos != std::string::npos)
    {
        auto destEnd = payload.find('\n', destPos);
        destination = payload.substr(destPos + 12, destEnd - destPos - 12);
    }

    // body 추출
    auto bodyPos = payload.find("\n\n");
    if (bodyPos == std::string::npos)
        return;

    std::string body = payload.substr(bodyPos + 2);
    if (!body.empty() && body.back() == '\0')
        body.pop_back();

    if (handlers.onMessage)
        handlers.onMessage(destination, body);
}