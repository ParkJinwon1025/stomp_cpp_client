#include "Session.hpp"
#include "Publisher.hpp"
#include "Subscriber.hpp"
#include <windows.h>

// ex02 : 1초마다 Timestamp를 보냄
int main()
{
    SetConsoleOutputCP(CP_UTF8);

    Session session("ws://localhost:9030/stomp/websocket");

    Subscriber sub;
    session.Subscribe("/topic/ubisam", &sub);

    session.Connect();

    Publisher pub;
    session.Publish("pub", &pub);

    std::cin.get();

    session.Disconnect();

    return 0;
}
