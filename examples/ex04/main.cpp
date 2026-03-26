#include "Session.hpp"
#include "Publisher.hpp"
#include "Reconnector.hpp"
#include "Subscriber.hpp"
#include <windows.h>

// ex04 : 임시 Reconnector 및 재연결 시 Subscribe 테스트
int main()
{
    // 콘솔 출력 인코딩을 UTF-8로 바꿈.
    SetConsoleOutputCP(CP_UTF8);

    Session session("ws://localhost:9030/stomp/websocket");

    Subscriber sub;
    session.Subscribe("/topic/ubisam", &sub);

    Reconnector reconnector(session);
    reconnector.Start();

    // 2개 이상의 Publihser와 Subscriber를 만들 때는 어떻게?

    session.Connect();

    Publisher pub;
    session.Publish("pub", &pub);

    std::cin.get();

    reconnector.Stop();
    session.Disconnect();

    return 0;
}
