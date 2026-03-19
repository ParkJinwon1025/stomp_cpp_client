#include "StompCore.hpp"
#include <iostream>
#include <mutex>

inline std::mutex coreCoutMutex;
#define CORE_LOG(msg)                                  \
    {                                                  \
        std::lock_guard<std::mutex> _l(coreCoutMutex); \
        std::cout << msg << std::endl;                 \
    }

StompCore::StompCore(Handlers handlers)
    : handlers(std::move(handlers))
{
}

StompCore::~StompCore()
{
    End();
}

std::string StompCore::ParseHost(const std::string &url) const
{
    auto start = url.find("://");
    if (start == std::string::npos)
        return "localhost";
    start += 3;
    auto end = url.find_first_of(":/", start);
    return url.substr(start, end - start);
}

void StompCore::Start(const std::string &url)
{
    if (stopRequested)
        return;

    uri = url;
    host = ParseHost(url);

    if (wsThread.joinable())
        wsThread.join();

    wsThread = std::thread([this]()
                           { TryConnect(); });
}

void StompCore::End()
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

bool StompCore::IsConnected() const
{
    std::lock_guard<std::mutex> lock(clientMutex);
    return currentClient != nullptr;
}

void StompCore::Pub(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

void StompCore::Sub(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

void StompCore::TryConnect()
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
            handlers.onConnect(c.get_io_service()); });

    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
        CORE_LOG("[CORE] raw received -> '" << msg->get_payload() << "'");
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

    c.set_pong_handler([this](websocketpp::connection_hdl, std::string)
                       {
        if (handlers.onPong)
            handlers.onPong(); });

    c.set_pong_timeout_handler([this](websocketpp::connection_hdl, std::string)
                               {
        if (handlers.onPongTimeout)
            handlers.onPongTimeout(); });

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