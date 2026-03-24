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
    session.Publish("pub", &pub);

    // Publisher1: Publish 내부에서 블로킹 (stdin 입력 대기) → 아래 불필요
    // Publisher2: Publish 즉시 리턴 → 엔터 누를 때까지 대기
    std::cin.get();

    session.Disconnect();

    return 0;
}
