#include "Session.hpp"
#include "Publisher.hpp"
#include "Subscriber.hpp"
#include <windows.h>

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
