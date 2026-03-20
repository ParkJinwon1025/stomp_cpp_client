# 수정 사항 정리

## 1. Session.hpp / Session.cpp

### 변경
- 기존 `Publish(dest, body)` → `Send(dest, body)` 로 이름 변경
- Session.cpp 내부에서 `Publish` 호출하는 곳도 `Send`로 변경

### 추가
- `void Send(const std::string& dest, const std::string& body)`
  - 문자열을 STOMP 프레임으로 마샬링 후 전송
- `template<typename T> void Send(const std::string& dest, const T& data)`
  - 구조체 → JSON → STOMP 프레임으로 마샬링 후 전송 (JSON 라이브러리 필요)
- `void Publish(Publisher* publisher)`
  - 내부에서 publisher->run() 호출

### Session.cpp RouteMessage 수정
- `s.subscriber->Received(body, *this)` → `s.subscriber->receive(body, *this)` 로 변경

---

## 2. Publisher.hpp / Publisher.cpp

### 변경 (전면 재작성)
- 기존: 워커 스레드, AddPeriodic, Enqueue 등 복잡한 구조
- 변경 후: 인터페이스만 제공, 구현은 사용자가 담당

**Publisher.hpp**
```cpp
class Publisher {
public:
    Publisher(Session& session);
    virtual void run() = 0;
    virtual ~Publisher() = default;
protected:
    Session& session;
};
```

**Publisher.cpp**
- 생성자만 구현

---

## 3. Subscriber.hpp

### 변경 (전면 재작성)
- 기존: 콜백 함수를 생성자에서 받아 저장하는 구조
- 변경 후: 순수 가상함수 인터페이스, 구현은 사용자가 담당

```cpp
class Subscriber {
public:
    virtual void receive(const std::string& body, Session& session) = 0;
    virtual ~Subscriber() = default;
};
```

---

## 4. main.cpp

### 변경
- 기존 복잡한 예제 제거
- 단순 예제로 교체:
  - PrintSubscriber 구현체 정의 (메시지 수신 시 "1" 출력)
  - session.Subscribe("/topic/test", &sub) 등록
  - session.Send("/topic/test", "hello") 단일 전송

---

## 작업 순서 (권장)

1. Publisher.hpp → 인터페이스로 재작성
2. Publisher.cpp → 생성자만 남기고 정리
3. Subscriber.hpp → 인터페이스로 재작성
4. Session.hpp → Send 추가, Publish(Publisher*) 추가
5. Session.cpp → Publish → Send 이름 변경, RouteMessage 수정, Publish(Publisher*) 구현
6. main.cpp → 단순 예제로 교체
