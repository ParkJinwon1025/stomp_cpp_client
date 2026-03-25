#include "Session.hpp"
#include "Publisher.hpp"
#include "Reconnector.hpp"
#include "Subscriber.hpp"
#include <windows.h>

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    Session session("ws://localhost:9030/stomp/websocket");

    Subscriber sub;
    session.Subscribe("/topic/ubisam", &sub);

    Reconnector reconnector(session);
    reconnector.Start();

    session.Connect();

    Publisher pub;
    session.Publish("pub", &pub);

    std::cin.get();

    reconnector.Stop();
    session.Disconnect();

    return 0;
}
