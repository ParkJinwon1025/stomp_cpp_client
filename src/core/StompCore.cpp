#include "StompCore.hpp"

// WebSocket 연결 해제
// STOMP 프레임 조립
// 이벤트 감지
// 핵심 통신 로직 담당

// 생성자
StompCore::StompCore(Handlers handlers)
    : handlers(std::move(handlers))
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
void StompCore::start(const std::string &url)
{
    // Stop이 호출되었을 경우 시작 안함
    if (stopRequested)
        return;

    // uri, host 설정
    uri = url;
    host = parseHost(url);

    // 이전 wsThread가 살아 있으면
    if (wsThread.joinable())
        wsThread.join(); // 이전 wsThread 완전히 끝날 때까지 대기

    // 새 Thread 생성
    wsThread = std::thread([this]()
                           { tryConnect(); });
}

// WebSocket 연결 종료 및 스레드 정리
void StompCore::stop()
{
    // 이미 Stop 했으면 중복실행 방지
    if (stopRequested.exchange(true))
        return;

    {
        // clientMutex 잠금 후 안전하게 접근
        std::lock_guard<std::mutex> lock(clientMutex);

        // currentClient가 연결 된 상태면
        if (currentClient)
            currentClient->stop();
    }

    // wsThread가 실행중이면
    if (wsThread.joinable())
        wsThread.detach(); // wsThread 분리 → 백그라운드에서 알아서 종료
}

// WebSocket 연결 상태 확인
bool StompCore::isConnected() const
{
    // 안전하게 currentClient 접근
    std::lock_guard<std::mutex> lock(clientMutex);

    // 연결된 상태면 true  / 연결 안 된 상태면 false
    return currentClient != nullptr;
}

// 실제 WebSocket 연결 및 이벤트 루프 실행
void StompCore::tryConnect()
{
    // websocket 클라이언트 객체 생성 ( 매 연결마다 새로 생성 )
    ws_client c;
    c.clear_access_channels(websocketpp::log::alevel::all);
    c.clear_error_channels(websocketpp::log::elevel::all);
    c.init_asio();

    // webSocket 연결 성공 시 실행할 핸들러
    c.set_open_handler([this, &c](websocketpp::connection_hdl h)
                       {
        {
            std::lock_guard<std::mutex> lock(clientMutex);

            // 현재 연결 핸들 저장
            hdl = h;

            // 현재 클라이언트 주소 저장, isConnected true 반환
            // => currentClient가 nullptr이 아니면 true 반환
            currentClient = &c;
        }
        sendConnectFrame(c); // STOMP 핸드셰이크

        if (handlers.onConnect)
            handlers.onConnect(); });

    // 메시지 수신 시 실행할 핸들러 등록
    c.set_message_handler([this](websocketpp::connection_hdl, ws_client::message_ptr msg)
                          {
                              parseMessage(msg->get_payload()); // 수신 메시지 파싱
                          });

    // 연결 끊김 시 실행할 핸들러 등록
    c.set_close_handler([this](websocketpp::connection_hdl)
                        {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr; // 연결 끊겼을 시 클라이언트 초기화
        }
        // 핸들러가 있으면 콜백함수 실행
        if (handlers.onDisconnect)
            handlers.onDisconnect(); });

    // 연결 실패 시 실행할 핸들러 등록
    c.set_fail_handler([this](websocketpp::connection_hdl)
                       {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            currentClient = nullptr; // 연결 실패시 클라이언트 초기화
        }

        // 핸들러가 있으면 콜백함수 실행
        if (handlers.onDisconnect)
            handlers.onDisconnect(); });

    websocketpp::lib::error_code ec;

    // uri로 객체 생성
    // 연결 준비 => 연결 실패하면 ec에 에러 담김
    auto con = c.get_connection(uri, ec);

    // uri로 객체 연결 생성
    if (ec)
    {
        // 연결 실패시 -> onDisconnect 호출
        if (handlers.onDisconnect)
            handlers.onDisconnect();
        return; // tryConnect 종료
    }

    // 에러 없으면 통과

    // 실제 연결 시도
    // 연결 성공하면 set_open_handler 실행
    c.connect(con);

    // 이벤트 루프 시작 ( 블로킹 )
    // 연결 상태를 유지하면서 이벤트 대기
    // Websocket 서버가 연결 끊거나 연결 실패하거나 stop 호출시 c.run이 종료됨. (감지할 대상이 없어서)
    // 새 데이터 수신(message handler), 연결 끊김(close_handler), 연결 실패(fail_handler), stop() 호출
    // 연결 끊으면 c.run()이 내부에서 감지해서 close_handler 실행 => currentClient = nullptr => onDisconnect 콜백 호출 => run 종료 => 블로킹 해제 => 쓰레드 종료
    // 서버에서 오는 STOMP 메시지 수신 및 연결 감지
    c.run();

    // 연결 끊김 or 연결 실패로 인한 c.run 블로킹 종료
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        currentClient = nullptr; // 종료 후 클라이언트 초기화
    }
}

// STOMP SEND 프레임 조립 및 전송
void StompCore::pub(const std::string &destination, const std::string &body)
{
    std::lock_guard<std::mutex> lock(clientMutex);

    // 연결 안된 상태면 종료 안함.
    if (!currentClient)
        return;

    // Send 프레임 조립
    std::string frame =
        "SEND\n"
        "destination:" +
        destination + "\n"
                      "content-type:application/json\n\n" +
        body;

    // 프레임 끝에 Null Terminator 추가
    frame.push_back('\0');

    // 전송 실패 시 에러 담을 변수 선언
    websocketpp::lib::error_code ec;

    // WebSocket으로 데이터를 전송
    // hdl : 어느 연결로 보낼지 식별하는 핸들
    // frame : 전송할 데이터
    // websocketpp::frame::opcode::text : 전송 형식
    // ec : 에러 담을 변수
    currentClient->send(hdl, frame, websocketpp::frame::opcode::text, ec);
}

// STOMP SUBSCRIBE 프레임 조립 및 전송
void StompCore::sub(const std::string &topic, const std::string &subId)
{
    std::lock_guard<std::mutex> lock(clientMutex);

    // 연결 안된 상태면 전송 안함.
    if (!currentClient)
        return;

    // STOMP Subscribe 프레임 조립
    std::string frame =
        "SUBSCRIBE\n"
        "id:" +
        subId + "\n"
                "destination:" +
        topic + "\n\n";
    // 프레임 끝에 Null Terminator 추가
    frame.push_back('\0');

    websocketpp::lib::error_code ec;
    currentClient->send(hdl, frame, websocketpp::frame::opcode::text, ec);
}

// STOMP 핸드셰이크 프레임 전송
// STOMP 연결 확인
void StompCore::sendConnectFrame(ws_client &c)
{
    std::string frame = "CONNECT\naccept-version:1.2\nhost:" + host + "\n\n";
    frame.push_back('\0');

    // 서버에 connect 프레임 전송
    c.send(hdl, frame, websocketpp::frame::opcode::text);
}

// raw payload를 STOMP 프레임으로 파싱 후 콜백 호출
void StompCore::parseMessage(const std::string &payload)
{
    // payload에서 첫 번쨰 \n 위치 찾기
    auto firstNewline = payload.find('\n');
    if (firstNewline == std::string::npos)
        return;

    // 첫 줄 추출
    std::string frameType = payload.substr(0, firstNewline);

    // STOMP 핸드셰이크 응답 → 무시
    if (frameType == "CONNECTED")
        return;

    // MESSAGE 프레임만 처리하고 나머지는 무시
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

    // body 추출 후에 끝에 붙은 NULL terminator 제거
    std::string body = payload.substr(bodyPos + 2);
    if (!body.empty() && body.back() == '\0')
        body.pop_back();

    // onMessage 콜백이 있으면
    if (handlers.onMessage)
        // onMessageHandler로 전달
        handlers.onMessage(destination, body);
}