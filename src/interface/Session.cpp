#include "Session.hpp"

Session::Session()
    : core(
          {// 연결 성공 시: CONNECT 프레임 전송 → 구독 재등록 → 콜백 호출
           [this](asio::io_service &ios)
           {
               ioService = &ios; // ASIO 이벤트 루프 저장 (Post에 사용)

               LOG("[SESSION] Connected -> Sending CONNECT frame");
               core.Pub(BuildConnectFrame()); // STOMP CONNECT 프레임 전송

               ReRegisterSubscriptions(); // 재연결 시 구독 목록 복구

           },
           // 연결 끊김 시: ioService, stompReady 초기화
           [this]()
           {
               LOG("[SESSION] Disconnected");
               ioService = nullptr;
               stompReady = false; // 재연결 전까지 큐 처리 중단
           },
           // 메시지 수신 시: 파싱 후 라우팅
           [this](const std::string &payload)
           {
               ParseMessage(payload);
           }})
{
}

Session::~Session()
{
    Disconnect(); // 소멸 시 자동 연결 종료
}

void Session::Init(const std::string &u)
{
    url = u;

    // URL에서 호스트명 추출 (예: ws://192.168.0.1:9030 → 192.168.0.1)
    auto pos = url.find("://");
    if (pos != std::string::npos)
    {
        pos += 3;
        auto end = url.find_first_of(":/", pos);
        host = url.substr(pos, end - pos);
    }
}

void Session::Connect()
{
    stopRequested = false;
    LOG("[SESSION] connect -> " << url);
    core.Start(url); // WebSocket 연결 시작 (별도 스레드)
}

void Session::Disconnect()
{
    if (stopRequested.exchange(true)) // 이미 종료 요청된 경우 중복 실행 방지
        return;

    LOG("[SESSION] disconnect");
    core.End(); // WebSocket 연결 종료
}

bool Session::IsConnected() const
{
    return core.IsConnected() && stompReady; // WebSocket + STOMP 둘 다 준비된 경우만 true
}

void Session::Publish(const std::string &destination, const std::string &body)
{
    core.Pub(BuildSendFrame(destination, body));
}

void Session::Subscribe(const std::string &topic, Subscriber *subscriber)
{
    LOG("[SESSION] subscribe -> " << topic);

    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, subscriber}); // 구독 목록에 추가
    }

    // 이미 연결 중이면 바로 SUBSCRIBE 프레임 전송
    if (IsConnected())
    {
        std::string subId = "sub-" + std::to_string(subscriptions.size() - 1);
        Post([this, topic, subId]()
             { core.Sub(BuildSubscribeFrame(topic, subId)); });
    }
}



void Session::Post(std::function<void()> task)
{
    if (!ioService) // 연결 끊긴 상태면 무시
        return;
    ioService->post(task); // ASIO 이벤트 루프에 작업 등록
}

void Session::ReRegisterSubscriptions()
{
    // 재연결 시 기존 구독 목록을 서버에 다시 등록
    std::lock_guard<std::mutex> lock(subMutex);
    for (size_t i = 0; i < subscriptions.size(); i++)
    {
        LOG("[SESSION] Re-subscribing -> " << subscriptions[i].topic);
        core.Sub(BuildSubscribeFrame(subscriptions[i].topic, "sub-" + std::to_string(i)));
    }
}

// ========================
// 프레임 빌드
// ========================

std::string Session::BuildConnectFrame() const
{
    // STOMP CONNECT 프레임 (heart-beat 비활성화)
    std::string frame =
        "CONNECT\n"
        "accept-version:1.1,1.2\n"
        "heart-beat:0,0\n\n";
    frame.push_back('\0'); // STOMP 프레임 종료 문자
    return frame;
}

std::string Session::BuildSendFrame(const std::string &dest, const std::string &body) const
{
    // STOMP SEND 프레임
    std::string frame =
        "SEND\n"
        "destination:" +
        dest + "\n"
               "content-type:application/json\n\n" +
        body;
    frame.push_back('\0'); // STOMP 프레임 종료 문자
    return frame;
}

std::string Session::BuildSubscribeFrame(const std::string &topic, const std::string &id) const
{
    // STOMP SUBSCRIBE 프레임
    std::string frame =
        "SUBSCRIBE\n"
        "id:" +
        id + "\n"
             "destination:" +
        topic + "\n\n";
    frame.push_back('\0'); // STOMP 프레임 종료 문자
    return frame;
}

// ========================
// 수신 처리
// ========================

void Session::ParseMessage(const std::string &payload)
{
    if (payload.empty())
        return;

    // 첫 줄에서 프레임 타입 추출 (CONNECTED, MESSAGE 등)
    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    std::string frameType = payload.substr(0, firstNewline);

    // STOMP 연결 확인 응답 처리
    if (frameType == "CONNECTED")
    {
        LOG("[SESSION] STOMP CONNECTED received");
        stompReady = true; // 이제부터 큐 처리 허용
        return;
    }

    // MESSAGE 프레임만 처리
    if (frameType != "MESSAGE")
        return;

    // 헤더에서 destination 추출
    std::string destination;
    auto destPos = payload.find("destination:");
    if (destPos != std::string::npos)
    {
        auto destEnd = payload.find('\n', destPos);
        destination = payload.substr(destPos + 12, destEnd - destPos - 12);
    }

    // 빈 줄(\n\n) 이후가 body
    auto bodyPos = payload.find("\n\n");
    if (bodyPos == std::string::npos)
        return;

    std::string body = payload.substr(bodyPos + 2);
    if (!body.empty() && body.back() == '\0') // 종료 문자 제거
        body.pop_back();

    LOG("[SESSION] MESSAGE received -> " << destination);

    // ASIO 스레드에서 라우팅 처리
    Post([this, destination, body]()
         { RouteMessage(destination, body); });
}

void Session::RouteMessage(const std::string &destination, const std::string &body)
{
    // destination과 일치하는 Subscriber 찾아서 콜백 호출
    std::lock_guard<std::mutex> lock(subMutex);
    for (const auto &s : subscriptions)
    {
        if (s.topic == destination && s.subscriber)
        {
            s.subscriber->Received(body, *this);
            break;
        }
    }
}
