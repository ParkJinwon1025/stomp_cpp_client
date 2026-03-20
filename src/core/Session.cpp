#include "Session.hpp"
#include "Publisher.hpp"
#include <iostream>

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

Session::~Session()
{
    Disconnect();
}

void Session::Connect()
{
    stopRequested = false;
    LOG("[SESSION] connect -> " << url);

    if (wsThread.joinable())
        wsThread.join();

    wsThread = std::thread([this]()
                           { TryConnect(); });
}

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

    if (wsThread.joinable())
        wsThread.detach();
}

bool Session::IsConnected() const
{
    std::lock_guard<std::mutex> lock(clientMutex);
    return currentClient != nullptr && stompReady;
}

void Session::Pub(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;
    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

void Session::Sub(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;
    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

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

void Session::Send(const std::string &destination, const std::string &body)
{
    // STOMP에 연결이 되지 않은 상태이면 대기 큐에 저장
    if (!stompReady)
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        pendingMessages.push_back({destination, body});
        LOG("[SESSION] Send queued (not ready) -> " << destination);
        return;
    }
    Pub(BuildSendFrame(destination, body));
}

void Session::Publish(Publisher *publisher)
{
    publisher->HandleStarted(*this);
}

void Session::Subscribe(const std::string &topic, Subscriber *subscriber)
{
    LOG("[SESSION] subscribe -> " << topic);
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, subscriber});
    }

    if (IsConnected())
    {
        std::string subId = "sub-" + std::to_string(subscriptions.size() - 1);
        Sub(BuildSubscribeFrame(topic, subId));
    }
}

void Session::Post(std::function<void()> task)
{
    if (!ioService)
        return;
    ioService->post(task);
}

void Session::ReRegisterSubscriptions()
{
    std::lock_guard<std::mutex> lock(subMutex);
    for (size_t i = 0; i < subscriptions.size(); i++)
    {
        LOG("[SESSION] Re-subscribing -> " << subscriptions[i].topic);
        Sub(BuildSubscribeFrame(subscriptions[i].topic, "sub-" + std::to_string(i)));
    }
}

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

        std::vector<std::pair<std::string, std::string>> pending;
        {
            std::lock_guard<std::mutex> lock(pendingMutex);
            pending = std::move(pendingMessages);
        }
        for (const auto &m : pending)
        {
            LOG("[SESSION] Flushing queued Send -> " << m.first);
            Pub(BuildSendFrame(m.first, m.second));
        }
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
