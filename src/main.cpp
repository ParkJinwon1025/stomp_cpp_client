#include "Session.hpp"
#include "Publisher.hpp"
#include "Reconnector.hpp"
#include "Subscriber.hpp"
#include <windows.h>
#include <string>
#include <memory>

// =============================================================================
// Subscriber 구현: /topic/robot 수신 처리
// =============================================================================

class RobotSubscriber : public Subscriber
{
public:
    void received(const std::string &body, Session &session) override
    {
        LOG("[ROBOT] received -> " << body);

        // 응답 발신 예시
        // session.publish("/app/ubisam", "{\"type\":\"ack\"}");
    }
};

// =============================================================================
// main
// =============================================================================

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    // 1) Session 생성 및 초기화
    Session session;
    session.init("ws://localhost:9030/stomp/websocket");

    // 2) 컴포넌트 생성
    Publisher publisher(session);
    Reconnector reconnector(session);

    // 3) 연결 성공 시 콜백 등록
    session.onConnected([&publisher]()
                        { publisher.flush(); });

    // 4) Subscriber 등록 - 람다 없이 직접 전달
    RobotSubscriber robotSub;
    session.subscribe("/topic/robot", &robotSub);

    // 5) 연결 시작
    session.connect();

    // 6) 컴포넌트 시작 (연결 실패해도 Reconnector가 재연결 시도)
    reconnector.start();
    publisher.start();

    // 7) 입력 루프
    LOG("[INPUT] Type message to send to /app/ubisam (type 'quit' to exit)");
    std::string input;
    while (std::getline(std::cin, input))
    {
        if (input == "quit")
            break;
        if (input.empty())
            continue;

        std::string body = "{\"type\":\"message\",\"data\":\"" + input + "\"}";
        publisher.enqueue("/app/ubisam", body);
    }

    // 8) 종료
    reconnector.stop();
    publisher.stop();
    session.disconnect();

    return 0;
}