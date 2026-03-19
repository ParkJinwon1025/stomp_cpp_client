#include "Session.hpp"     // WebSocket + STOMP 연결 관리
#include "Publisher.hpp"   // 주기적 전송 및 즉시 전송 담당
#include "Reconnector.hpp" // 연결 끊김 감지 및 자동 재연결
#include "Subscriber.hpp"  // 메시지 수신 콜백 인터페이스
#include <windows.h>       // SetConsoleOutputCP (한글 출력)
#include <string>

int main()
{
    SetConsoleOutputCP(CP_UTF8); // 콘솔 UTF-8 출력 설정

    // 1) Session 생성 및 초기화
    Session session;
    session.Init("ws://localhost:9030/stomp/websocket"); // 서버 URL 설정

    // 2) 컴포넌트 생성 (session을 공유해서 사용)
    Publisher publisher(session);
    Reconnector reconnector(session);

    // 3) 주기적 전송 등록 (Start 전에 등록해야 함)

    // 메시지 업데이트 불가
    // 메시지가 업데이트되는
    // remove는 없다
    // change는 없다
    // String만 전송이 가능
    // 유동적인 메시지 전송
    publisher.AddPeriodic("/app/ubisam", "{\"type\":\"heartbeat\",\"message\":\"imalive\"}", 10, true); // 10초마다 heartbeat (timestamp 포함)
    //     .AddPeriodic("/app/ubisam", "{\"type\":\"status\",\"message\":\"on\"}", 5);                    // 5초마다 status

    // 4) Subscriber 등록: 토픽별 수신 콜백 정의
    Subscriber robotSub([&publisher](const std::string &body, Session &session)
                        {
                            LOG("[ROBOT] received -> " << body);
                            publisher.Enqueue("/app/ubisam", body); // 큐를 통해 전송 (순서 보장)
                        });

    Subscriber robot1Sub([&publisher](const std::string &body, Session &session)
                         {
                             LOG("[ROBOT1] received -> " << body);
                             publisher.Enqueue("/app/ubisam", body); // 큐를 통해 전송 (순서 보장)
                         });

    session.Subscribe("/topic/robot", &robotSub);   // /topic/robot 구독
    session.Subscribe("/topic/robot1", &robot1Sub); // /topic/robot1 구독

    // 5) 연결 시작
    session.Connect();

    // 6) 컴포넌트 시작
    reconnector.Start(); // 연결 감시 스레드 시작
    publisher.Start();   // 발신 워커 스레드 시작

    // 7) 입력 루프: 사용자 입력을 즉시 전송
    LOG("[INPUT] Type message to send to /app/ubisam (type 'quit' to exit)");
    std::string input;
    while (std::getline(std::cin, input))
    {
        if (input == "quit")
            break;
        if (input.empty())
            continue;

        std::string body = "{\"type\":\"message\",\"data\":\"" + input + "\"}";
        publisher.Enqueue("/app/ubisam", body); // 큐에 추가 → 워커가 전송
    }

    // 8) 종료: 역순으로 정리
    reconnector.Stop();
    publisher.Stop();
    session.Disconnect();

    return 0;
}
