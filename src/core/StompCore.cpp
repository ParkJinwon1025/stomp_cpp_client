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

// 웹소켓 연결 시작
void StompCore::start()
{
    if (stopRequested)
        return;

    if (wsThread.joinable())
        wsThread.join();

    wsThread = std::thread([this]()
                           { tryConnect(); });
}

// 웹소켓 연결 종료 및 스레드 정리
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
        sendConnectFrame(); // STOMP 핸드셰이크

        if (handlers.onConnect)
            handlers.onConnect(); });

    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
        // 파싱 없이 raw payload 그대로 Interface로 전달
        if (handlers.onRawMessage)
            handlers.onRawMessage(msg->get_payload()); });

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

// raw 프레임 전송
void StompCore::sendRaw(const std::string &frame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, frame, websocketpp::frame::opcode::text, ec);
}

// STOMP 핸드셰이크 프레임 전송
void StompCore::sendConnectFrame()
{
    std::string frame = "CONNECT\naccept-version:1.2\nhost:" + host + "\n\n";
    frame.push_back('\0');
    sendRaw(frame);
}