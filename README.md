# stomp_cpp_client

C++ STOMP over WebSocket 클라이언트 라이브러리

## 요구사항

- VSCode
- MSYS2 + MinGW-w64 (g++ 컴파일러)
- CMake 3.15 이상
- vcpkg
- websocketpp (프로젝트 내 포함)

## 환경 설정 (처음 설치하는 경우)

### 1. VSCode 설치

[VSCode 다운로드](https://code.visualstudio.com/)에서 설치 후, 아래 확장을 설치하세요.

- **C/C++** (Microsoft)
- **CMake Tools** (Microsoft)

### 2. MSYS2 설치 (g++ 컴파일러)

[VSCode MinGW 가이드](https://code.visualstudio.com/docs/cpp/config-mingw)를 따라 MSYS2와 MinGW-w64를 설치하세요.

설치 후 MSYS2 터미널에서:

```bash
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```

> 설치 후 `C:\msys64\ucrt64\bin` 을 시스템 PATH에 추가하세요.

### 3. CMake 설치

[CMake 다운로드](https://cmake.org/download/)에서 Windows 설치파일을 받아서 설치하세요.

> 설치 시 **"Add CMake to the system PATH"** 옵션을 선택하세요.

### 4. vcpkg 설치

```bash
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
cd C:/vcpkg
bootstrap-vcpkg.bat
```

### 5. 패키지 설치

```bash
C:/vcpkg/vcpkg.exe install asio:x64-mingw-dynamic
C:/vcpkg/vcpkg.exe install nlohmann-json:x64-mingw-dynamic
C:/vcpkg/vcpkg.exe install openssl:x64-mingw-dynamic
```

## 빌드

```bash
git clone <repo-url>
cd stomp_cpp_client
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build .
```

## 실행

빌드 후 `build/` 폴더 안에 예제별 실행 파일이 생성됩니다.

```bash
# Windows
build\ex01.exe
build\ex02.exe
build\ex03.exe
build\ex04.exe
```

## 예제 목록

| 예제 | 설명 |
|------|------|
| ex01 | 터미널 입력 → JSON으로 변환 후 전송 (`q` 입력 시 종료) |
| ex02 | 1초마다 timestamp를 JSON으로 전송 |
| ex03 | 수신한 JSON의 `action` 값에 따라 응답 전송 (move/stop) |
| ex04 | 1초마다 struct → JSON 자동 변환 후 전송 + 자동 재연결 |

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

## 서버 URL 변경

각 예제의 `main.cpp`에서 수정:

```cpp
Session session("ws://localhost:9030/stomp/websocket");
```
