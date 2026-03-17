#include "StompCore.hpp"

StompCore::StompCore(Handlers handlers)
    : handlers(std::move(handlers))
{
}

StompCore::~StompCore()
{
    stop();
}

std::string StompCore::parseHost(const std::string &url) const
{
    auto start = url.find("://");
    if (start == std::string::npos)
        return "localhost";
    start += 3;
    auto end = url.find_first_of(":/", start);
    return url.substr(start, end - start);
}

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

bool StompCore::isConnected() const
{
    std::lock_guard<std::mutex> lock(clientMutex);
    return currentClient != nullptr;
}

void StompCore::send(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

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
            handlers.onConnect(); });

    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
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