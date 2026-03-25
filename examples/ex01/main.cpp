#include "Session.hpp"
#include "Publisher.hpp"
#include "Reconnector.hpp"
#include "Subscriber.hpp"
#include <windows.h>

// ex01. 사용자가 터미널에 값을 입력하면 그 값을 /app/Ubisam에 전송
int main()
{
    SetConsoleOutputCP(CP_UTF8);

    Session session("ws://localhost:9030/stomp/websocket");

    session.Connect();

    Subscriber sub;
    session.Subscribe("/topic/ubisam", &sub);

    Publisher pub;
    session.Publish("pub", &pub);

    session.Disconnect();

    return 0;
}
