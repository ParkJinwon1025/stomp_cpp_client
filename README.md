# stomp_cpp_client

C++ STOMP over WebSocket 클라이언트 라이브러리

> C++ 개발 환경이 처음이라면 [VSCode C++ 환경 설정 가이드](https://code.visualstudio.com/docs/cpp/config-mingw)를 먼저 참고하세요.

## 프로젝트 구조

```
stomp_cpp_client/
├── lib/                  # 라이브러리 헤더 및 구현
│   ├── Session.hpp
│   ├── Session.cpp
│   ├── Publisher.hpp
│   ├── Subscriber.hpp
│   ├── Reconnector.hpp
│   └── Reconnector.cpp
├── examples/
│   ├── ex01/
│   │   ├── main.cpp
│   │   ├── Publisher.cpp
│   │   └── Subscriber.cpp
│   ├── ex02/
│   │   ├── main.cpp
│   │   ├── Publisher.cpp
│   │   └── Subscriber.cpp
│   ├── ex03/
│   │   ├── main.cpp
│   │   └── Subscriber.cpp
│   └── ex04/
│       ├── main.cpp
│       ├── Publisher.cpp
│       └── Subscriber.cpp
├── websocketpp/          # 프로젝트 내 포함
└── CMakeLists.txt
```

## 예제 목록

| 예제 | 설명 |
|------|------|
| ex01 | 터미널 입력 → JSON으로 변환 후 전송 (`q` 입력 시 종료) |
| ex02 | 1초마다 timestamp를 JSON으로 전송 |
| ex03 | 수신한 JSON의 `action` 값에 따라 응답 전송 (move/stop) |
| ex04 | 1초마다 struct → JSON 자동 변환 후 전송 + 자동 재연결 |

## 요구사항

- VSCode
- CMake 3.15 이상
- vcpkg
- websocketpp (프로젝트 내 포함)

## 환경 설정 (처음 설치하는 경우)

### 1. vcpkg 설치

```bash
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
cd C:/vcpkg
bootstrap-vcpkg.bat
```

### 2. 패키지 설치

```bash
C:/vcpkg/vcpkg.exe install asio:x64-windows
C:/vcpkg/vcpkg.exe install nlohmann-json:x64-windows
C:/vcpkg/vcpkg.exe install openssl:x64-windows
```

> vcpkg를 다른 경로에 설치했다면 `C:/vcpkg` 부분을 해당 경로로 변경하세요.

## 빌드

```bash
git clone <repo-url>
cd stomp_cpp_client
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

> vcpkg 경로가 다르다면 `-DCMAKE_TOOLCHAIN_FILE` 값을 해당 경로로 변경하세요.

## 실행

빌드 후 `build/Debug/` 폴더 안에 예제별 실행 파일이 생성됩니다.

```bash
# Windows
Debug\ex01.exe
Debug\ex02.exe
Debug\ex03.exe
Debug\ex04.exe
```

## 서버 URL 변경

각 예제의 `main.cpp`에서 수정:

```cpp
Session session("ws://localhost:9030/stomp/websocket");
```
