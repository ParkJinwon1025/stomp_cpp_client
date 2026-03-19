#include "StompCore.hpp"
#include <iostream>
#include <mutex>

// 코어 내부 전용 로그 뮤텍스 (Session의 coutMutex와 별도)
// mutex : 다중 스레드 환경에서 공유 자원에 대한 접근을 한 번에 하나의 스레드로 제한함.
inline std::mutex coreCoutMutex;

// 매크로
//
#define CORE_LOG(msg)                                  \
    {                                                  \
        std::lock_guard<std::mutex> _l(coreCoutMutex); \
        std::cout << msg << std::endl;                 \
    }

// 생성자
// 핸들러를 내부 변수 handlers에 저장
StompCore::StompCore(Handlers handlers)
    : handlers(std::move(handlers)) // 핸들러를 이동 생성으로 저장
{
}

// 객체가 사라질 때 자동으로 연결 종료
StompCore::~StompCore()
{
    End(); // 소멸 시 자동 연결 종료
}

// URL에서 호스트 추출
std::string StompCore::ParseHost(const std::string &url) const
{
    // "ws://host:port/path" → "host" 추출
    auto start = url.find("://");
    if (start == std::string::npos)
        return "localhost";
    start += 3;
    auto end = url.find_first_of(":/", start);
    return url.substr(start, end - start);
}

// STOMP 서버 연결 시작
void StompCore::Start(const std::string &url)
{
    if (stopRequested) // 종료 요청 상태면 연결 불가
        return;

    if (IsConnected()) // 이미 연결 중이면 무시
        return;

    uri = url;
    host = ParseHost(url);

    if (wsThread.joinable())
        wsThread.join(); // 이전 스레드 정리

    // 별도 스레드에서 WebSocket 연결 시도
    // STOMP 서버에 웹 소켓 연결
    wsThread = std::thread([this]()
                           { TryConnect(); });
}

// 연결 종료
void StompCore::End()
{
    if (stopRequested.exchange(true)) // 이미 종료 중이면 중복 실행 방지
        return;

    {
        // currentClient에 접근할 때 멀티스레드 안전 보장
        std::lock_guard<std::mutex> lock(clientMutex);
        if (currentClient)
            currentClient->stop(); // WebSocket 이벤트 루프 중단
    }

    if (wsThread.joinable())
        wsThread.detach(); // 스레드 분리 (강제 종료)
}

// 연결 상태 확인
bool StompCore::IsConnected() const
{
    std::lock_guard<std::mutex> lock(clientMutex);
    return currentClient != nullptr; // currentClient가 있으면 연결 중
}

// 메시지 발행
void StompCore::Pub(const std::string &rawFrame)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    if (!currentClient)
        return;

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, rawFrame, websocketpp::frame::opcode::text, ec);
}

// 구독
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
    ws_client c;                                            // WebSocket 클라이언트 생성 (스택에 할당, 스레드 로컬)
    c.clear_access_channels(websocketpp::log::alevel::all); // websocketpp 접근 로그 비활성화
    c.clear_error_channels(websocketpp::log::elevel::all);  // websocketpp 에러 로그 비활성화

    c.init_asio(); // ASIO 이벤트 루프 초기화

    // this 포인터 : this 포인터를 캡처하면 람다 안에서 this->로 멤버에 접근 가능
    // 연결 성공 시: currentClient, ioService 설정 후 onConnect 콜백 호출
    c.set_open_handler([this, &c](websocketpp::connection_hdl h)
                       {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            hdl = h; // 연결의 핸들 저장
            currentClient = &c; // WebSocket 클라이언트 포인터 저장
            ioService = &c.get_io_service(); // Pub/Sub에서 io_service 스레드로 전송하기 위해 저장
        }
        if (handlers.onConnect)
            handlers.onConnect(c.get_io_service()); }); // 상위 레이어에 연결됐다고 알림

    // 메시지 수신 시: onRawMessage 콜백 호출
    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
        CORE_LOG("[CORE] raw received -> '" << msg->get_payload() << "'");
        if (handlers.onRawMessage)
            handlers.onRawMessage(msg->get_payload()); });

    // 연결 정상 종료 시: currentClient, ioService 초기화 후 onDisconnect 호출
    c.set_close_handler([this](websocketpp::connection_hdl)
                        {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr;
            ioService = nullptr;
        }
        if (handlers.onDisconnect)
            handlers.onDisconnect(); });

    // 연결 실패 시: currentClient, ioService 초기화 후 onDisconnect 호출
    c.set_fail_handler([this](websocketpp::connection_hdl)
                       {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr;
            ioService = nullptr;
        }
        if (handlers.onDisconnect)
            handlers.onDisconnect(); });

    websocketpp::lib::error_code ec;
    auto con = c.get_connection(uri, ec); // 연결 객체 생성
    if (ec)
    {
        // URL 파싱 등 초기 연결 생성 실패
        if (handlers.onDisconnect)
            handlers.onDisconnect();
        return;
    }

    c.connect(con); // 연결 시작
    c.run();        // ASIO 이벤트 루프 실행 (연결 종료될 때까지 블로킹)

    // c.run() 종료 = 연결 끊김 → currentClient 초기화
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        currentClient = nullptr;
    }
}
