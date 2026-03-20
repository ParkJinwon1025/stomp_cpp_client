#include "Session.hpp"
#include "Publisher.hpp"
#include "Reconnector.hpp"
#include "Subscriber.hpp"
#include <windows.h>

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    Session session;
    session.Init("ws://localhost:9030/stomp/websocket");

    Subscriber sub;
    session.Subscribe("/topic/ubisam", &sub);

    session.Connect();

    Publisher pub(session);
    session.Publish(&pub);

    session.Disconnect();

    return 0;
}
