#include "Session.hpp"
#include "Publisher.hpp"
#include "Reconnector.hpp"
#include "Subscriber.hpp"
#include <windows.h>

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    Session session("ws://localhost:9030/stomp/websocket");

    session.Connect();

    Publisher pub;
    session.Publish(&pub);

    session.Disconnect();

    return 0;
}
