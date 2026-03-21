# 대화 내용 정리 (2026-03-20)

## 1. 쓰레드 분리 구조

Publisher 내부에서 쓰레드 시작과 내용을 분리한다.

```cpp
// Publisher.hpp
class Publisher {
public:
    void HandleStarted(Session &session); // 쓰레드 시작만 담당 (detach)
private:
    void Run(Session &session);           // 실제 쓰레드 내용
};
```

```cpp
// Publisher.cpp
void Publisher::HandleStarted(Session &session) {
    std::thread(&Publisher::Run, this, std::ref(session)).detach(); // 시작
}

void Publisher::Run(Session &session) {
    while (true) {
        // 실제 로직...
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

---

## 2. JSON 핸들링 두 가지 방식

### 1안 — nlohmann::json 객체 사용 (Publisher3)

- **JSON 객체** = `nlohmann::json` 타입
- json 객체에 필드를 추가한 뒤 `.dump()`로 문자열 변환
- Publisher3에서 `nlohmann::json` 객체를 그대로 Send에 넘기고, **Session::Send 내부에서 dump()**

```cpp
// Publisher3
nlohmann::json j;
j["timestamp"] = ms;
j["value"]     = 42;
j["source"]    = "sensor-A";

session.Send("/app/ubisam", j); // json 객체를 넘김
```

```cpp
// Session::Send (1안용 오버로드)
void Session::Send(const std::string &destination, const nlohmann::json &j) {
    std::string body = j.dump(); // 내부에서 문자열 변환
    Pub(BuildSendFrame(destination, body));
}
```

### 2안 — struct → JSON 객체 → 문자열 (Publisher4)

- struct를 정의하고, `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` 매크로로 자동 변환 등록
- struct → json 객체 → dump() 순서로 변환
- Publisher4에서 struct를 그대로 Send에 넘기고, **Session::Send 내부에서 변환**

```cpp
// struct 정의
struct SensorData {
    long long   timestamp;
    int         value;
    std::string source;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SensorData, timestamp, value, source)
```

```cpp
// Publisher4
SensorData data{ ms, 42, "sensor-A" };
session.Send("/app/ubisam", data); // struct를 넘김
```

```cpp
// Session::Send (2안용 — 기존 template 활용)
template <typename T>
void Session::Send(const std::string &destination, const T &data) {
    nlohmann::json j = data; // struct → json 자동 변환
    std::string body = j.dump();
    Pub(BuildSendFrame(destination, body));
}
```

---

## 3. 핵심 정리

| | 1안 (Publisher3) | 2안 (Publisher4) |
|---|---|---|
| Publisher에서 넘기는 것 | `nlohmann::json` 객체 | `struct` |
| Session::Send 내부 처리 | `j.dump()` | `json j = data; j.dump()` |
| 최종 전송 타입 | `std::string` | `std::string` |
| 결과 문자열 | 동일 | 동일 |

- **결국 전송되는 건 문자열** — 두 방식 모두 동일
- **차이는 문자열을 만드는 과정**
- **변환 책임은 Session::Send 내부** — Publisher는 그냥 넘기기만 함
- Send는 오버로딩으로 각각 따로 존재 (1안용, 2안용)

---

## 4. "JSON 라이브러리를 스트럭트를 반영하는지 연구" 의미

2안 구현 시 라이브러리가 struct → JSON 자동 변환을 지원하는지 확인하라는 뜻.
→ **nlohmann/json은 지원함** (`NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` 매크로)

---

## 5. Publisher 구조 요약

| Publisher | 역할 |
|---|---|
| Publisher1 | 키보드 입력 → 문자열 직접 Send |
| Publisher2 | 1초마다 timestamp → 문자열 직접 Send (detach) |
| Publisher3 | 1초마다 → **nlohmann::json 객체** → Send (1안) |
| Publisher4 | 1초마다 → **struct** → Send (2안) |

---

## 6. 역할 분리 원칙

- **Session::Send** — json 객체 또는 struct를 받아서 문자열로 변환 후 전송 (인프라 역할)
- **Publisher** — 사용자가 직접 json 객체나 struct를 만들어서 Send에 넘기는 곳 (비즈니스 로직)

```
Send는 "어떻게 보낼지" 담당
Publisher는 "무엇을 보낼지" 담당
```

→ struct나 json 구성은 사용자가 Publisher에 직접 구현하고, Send는 받아서 처리만 한다.
