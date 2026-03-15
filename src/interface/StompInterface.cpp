#include "StompInterface.hpp"

// Core와 Interface 연결
StompInterface::StompInterface(const std::string &url, Handlers handlers)
    : handlers(std::move(handlers)),
      core(url,
           {[this]()
            {
                // 연결 시 콜백 호출
                if (this->handlers.onConnect)
                    this->handlers.onConnect();
            },
            [this]()
            {
                // 해제 시 콜백 호출
                if (this->handlers.onDisconnect)
                    this->handlers.onDisconnect();
            },
            [this](const std::string &destination, const std::string &body)
            {
                onMessageHandler(destination, body); // 메시지 라우팅
            }})
{
}

// 자동 정리
StompInterface::~StompInterface()
{
    stop();
}

// Core에 시작 명령
void StompInterface::start()
{
    // stopRequested 초기화
    // 시작 상태라 stopRequested false로 설정
    stopRequested = false;
    core.start();
}

// Core에 종료 명령
void StompInterface::stop()
{
    // 중복 호출 방지
    // stopRequested : false  => 아직 stop 안해서 지금 stop 시켜야함
    // stopRequested : true => 이미 stop해서 중복이니까 막음(return)
    if (stopRequested.exchange(true))
        return;
    core.stop();
}

// Core에 연결 상태 확인 위임
bool StompInterface::isConnected() const
{
    return core.isConnected();
}

// 연결 확인 후 Core에 전송 명령
void StompInterface::pub(const std::string &destination, const std::string &body)
{
    if (!core.isConnected())
        return;
    core.pub(destination, body);
}

// 구독 목록 저장 후 Core에 구독 명령
// 1. sub 호출
// 2. subscriptions에 callback 추가
// 3. sub 호출
// 4. 서버에 Subscribe 프레임 전송
void StompInterface::sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback)
{
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, callback});
    }
    core.sub(topic, "sub-" + std::to_string(subscriptions.size() - 1));
}

// 수신 메시지를 구독 목록에서 찾아 콜백 실행
void StompInterface::onMessageHandler(const std::string &destination, const std::string &body)
{

    // lock_guard : 잠금을 자동을 관리하는 RAII 클래스
    // 잠굴 대상 : subMutext
    // 여러 쓰레드가 subscriptions에 접근하는 걸 막아 데이터 충돌 방지
    std::lock_guard<std::mutex> lock(subMutex); // 멀티 쓰레드에서 안전하게 subscriptions 접근
    for (const auto &s : subscriptions)         // 구독 목록 순회
    {
        if (s.topic == destination && s.callback) // 일치하는 콜백 있으면
        {
            s.callback(destination, body); // 해당 callback 실행
            break;
        }
    }
}