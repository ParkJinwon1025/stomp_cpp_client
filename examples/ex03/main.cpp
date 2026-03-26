#include "Session.hpp"
#include "Publisher.hpp"
#include "Reconnector.hpp"
#include "Subscriber.hpp"
#include <windows.h>

// ex03 : 요청(ex] { "action" : "move/stop" })을 주면 action 값에 따라 응답을 줌
int main()
{
    // 콘솔 출력 인코딩을 UTF-8로 바꿈.
    SetConsoleOutputCP(CP_UTF8);

    Session session("ws://localhost:9030/stomp/websocket");

    Subscriber sub;
    session.Subscribe("/topic/robot", &sub);

    session.Connect();

    std::cin.get();

    session.Disconnect();
    return 0;
}
