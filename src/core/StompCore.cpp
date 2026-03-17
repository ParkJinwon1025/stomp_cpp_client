#include "StompCore.hpp"

// WebSocket 연결/해제, raw 전송만 담당
// STOMP 관련 로직은 StompInterface로 이동

StompCore::StompCore(Handlers handlers)
    : handlers(std::move(handlers))
{
}

StompCore::~StompCore()
{
    stop();
}

// URL에서 호스트명 추출 (start 내부용)
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
void StompCore::start(const std::string &url)
{
    if (stopRequested)
        return;

    uri = url;
    host = parseHost(url);

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

// raw string 전송만 담당
void StompCore::send(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
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
                           if (handlers.onConnect)
                               handlers.onConnect(); // STOMP CONNECT 프레임은 Interface에서 처리
                       });

    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
                              if (handlers.onRawMessage)
                                  handlers.onRawMessage(msg->get_payload()); // raw payload만 위로 올림
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