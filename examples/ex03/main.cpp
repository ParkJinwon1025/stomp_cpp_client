#include "Session.hpp"
#include "Publisher.hpp"
#include "Reconnector.hpp"
#include "Subscriber.hpp"
#include <windows.h>

// ex03 : 요청(ex] { "action" : "move/" })을 주면 action 값에 따라 응답을 줌
int main()
{
    SetConsoleOutputCP(CP_UTF8);

    Session session("ws://localhost:9030/stomp/websocket");

    Subscriber sub;
    session.Subscribe("/topic/robot", &sub);

    session.Connect();

    std::cin.get();

    session.Disconnect();
    return 0;
}
