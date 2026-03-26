# websocketpp 지원 여부 및 구현 방식

---

## 1. Connect

**websocketpp 지원 여부: O**

- 참조: `tutorials/utility_client/step4.cpp`

| 단계 | 호출 | 위치 |
|------|------|------|
| 연결 객체 생성 | `client.get_connection(uri, ec)` | step4.cpp:106 |
| 핸들러 등록 | `set_open_handler`, `set_fail_handler` | step4.cpp:117~128 |
| 연결 실행 | `client.connect(con)` | step4.cpp:130 |

**우리 코드 구현 (Session.cpp)**
- `get_connection()`, `connect()` 를 그대로 사용
- `set_open_handler` 안에서 추가로 STOMP CONNECT 프레임을 직접 조립해 `send()` 로 전송
- `set_message_handler` 안에서 STOMP CONNECTED 응답을 확인 후 `stompReady = true` 설정

---

## 2. Disconnect

**websocketpp 지원 여부: O**

- 참조: `tutorials/utility_client/step5.cpp`

| 단계 | 호출 | 위치 |
|------|------|------|
| 연결 닫기 | `client.close(hdl, going_away, "", ec)` | step5.cpp:137, 194 |
| 이벤트 루프 종료 | `client.stop_perpetual()` | step5.cpp:126 |
| 스레드 정리 | `thread.join()` | step5.cpp:144 |

**우리 코드 구현 (Session.cpp)**
- `Disconnect()` → `client.close()` 그대로 사용 (Session.cpp:277)
- 소멸자에서 `stop_perpetual()` + `wsThread.join()` 으로 정리 (Session.cpp:82~94)

---

## 3. IsConnected

**websocketpp 지원 여부: X**

websocketpp는 연결 상태를 조회하는 메서드를 제공하지 않음.
`set_open_handler`, `set_close_handler`, `set_fail_handler` 콜백만 제공.

**우리 코드 구현 (Session.cpp)**
- `std::atomic<bool> stompReady` 플래그를 직접 관리
- STOMP CONNECTED 프레임 수신 시 `true` (Session.cpp:148)
- close / fail 핸들러에서 `false` (Session.cpp:189, 195)
- `IsConnected()` 가 이 플래그를 반환 (Session.cpp:284)

---

## 4. Publish (Send)

**websocketpp 지원 여부: X (전송 수단만 제공)**

websocketpp는 `client.send(hdl, payload, opcode, ec)` 로 raw bytes 전송만 지원.
STOMP 프레임 개념 없음.

**우리 코드 구현 (Session.cpp)**
- STOMP SEND 프레임을 문자열로 직접 조립 후 `client.send()` 호출 (Session.cpp:214~226)

```
SEND
destination:/queue/orders
content-type:application/json

{payload}\0
```

---

## 5. Subscribe

**websocketpp 지원 여부: X**

websocketpp는 Subscribe 개념이 없음.
단일 `set_message_handler` 콜백으로 모든 수신 메시지를 받을 뿐.

**우리 코드 구현 (Session.cpp)**
- STOMP SUBSCRIBE 프레임을 직접 조립해 `client.send()` 로 전송 (Session.cpp:257~268)
- 수신 메시지에서 `destination` 헤더를 파싱해 `subscribers_` 맵으로 직접 라우팅 (Session.cpp:156~182)

```
SUBSCRIBE
id:sub-0
destination:/topic/chat

\0
```
