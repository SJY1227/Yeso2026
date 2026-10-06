# 입력·절전·시간 동기화 계약

## 이번 사용자 요구

- 현재 버튼 1개에서는 연속 누르기·유지 시간으로 입력을 구분한다. 이후 버튼 3개로 바뀔 때 제품 로직을 다시 쓰지 않는다.
- 현재 배선으로 절전을 구현한다. 과거 소스 확보나 BL 추가 배선을 전제하지 않는다.
- 절전이 풀릴 때마다 Wi-Fi로 새 시각을 받아 시계를 맞추는 동작을 항상 호출한다.
- 처음 설정한 Wi-Fi가 있는 장소에서 계속 사용한다. 매번 설정을 받지 않고 같은 설정으로 재접속한다.

## 입력의 교체 지점

`GPIO → Button 디바운스 → Gestures → mapSingleButton → Intent → 화면의 조작 처리`

| 의도 | 현재 D3 버튼 | 3개 버튼 구성의 예 |
|---|---|---|
| Previous | 두 번 짧게 누름 | Primary(왼쪽) 버튼 짧게 누름 |
| Next | 한 번 짧게 누름 | Tertiary(오른쪽) 버튼 짧게 누름 |
| Confirm | 800ms 유지 | Secondary(가운데) 버튼 짧게 누름 |

매핑은 [Intent.h](../firmware/RoutineDevice/src/input/Intent.h)의 두 함수에 모았다. 사용자가 확정한 의도는 이전 선택·다음 선택·확인이다. 이전의 Back 의도는 폐기했다. 포기와 취소는 선택 커서와 확인창에서 처리하며, [화면별 조작](product-runtime.md)을 따른다.

단일 탭은 두 번째 누르기를 기다리는 300ms 후 전달한다. 두 번째 눌림 시작이 그 안에 있으면 한 번의 DoubleTap으로 전달하고 첫 탭은 따로 실행하지 않는다. 첫 탭 다음에 길게 누르면 Hold 하나만 전달한다. 세 번 누르기는 별도 기능이 아니며 DoubleTap + Tap 두 입력이다. 디바운스 30ms, 두 번 누르기 300ms, 길게 800ms는 입력 계층의 조정 가능한 값이다.

3개 버튼으로 바꿀 때 `Board::inputTask()`에서 핀별 `Button`을 읽고 ShortPress를 `mapThreeButtons(control, Gesture::Tap)`에 보낸다. 이 경우 두 번 누르기 판별 대기가 필요 없다. 추가 버튼 핀·깨우기 핀은 실제 배선 시 정의한다. 화면과 루틴 엔진의 함수는 그대로 둔다.

부팅/절전 복귀 당시 눌린 버튼은 놓을 때까지 무시한다. 절전 진입 전 처리 중인 제스처가 있으면 진입하지 않는다. 한 개 버튼뿐이므로 여러 버튼 동시 입력의 우선순위는 아직 정하지 않았다.

## 절전 경계

제품에서 사용할 진입점은 [PowerSession.h](../firmware/RoutineDevice/src/platform/esp32/PowerSession.h)의 `sleepAndResync(board, clock, seconds)`다.

1. 호출하는 제품 애플리케이션은 먼저 진행 중인 상태 변경을 저장하고 다음 예약 경계까지의 기상 시간을 계산한다.
2. 입력/부저 처리 중이면 진입을 거절한다. 시간 동기화 작업을 중지하고 Wi-Fi를 끈다. 저장한 Wi-Fi 설정은 지우지 않는다.
3. LCD 표시 중지·sleep-in 명령 후 ESP32 light sleep에 들어간다. D3 LOW와 타이머가 깨우기 원인이다.
4. RAM과 프레임 버퍼를 유지한 채 돌아온다. 깨우기 입력을 초기화하고 LCD를 복구한다.
5. **버튼/타이머 등 기상 원인과 관계없이 `clock.requestSync()`를 호출한다.** Wi-Fi를 끈 뒤 하드웨어 절전이 실패한 경우도 연결을 복구한다.

현재는 light sleep이다. 재부팅 후 제품 상태 복원은 v0.4의 FAT/NVS 저장 계층에 연결했다. deep sleep은 사용하지 않는다. 화면에 절전 명령을 보냈다는 사실과 실물 밝기·전류 측정 결과는 구분한다. 현재 배선에서 화면 절전이 성공했다는 사용자 확인은 유지한다.

개발용 Hardware CDC는 수동 light sleep 동안 USB 트랜잭션에 응답하지 못하고, 복귀해도 PC가 다시 인식하지 않을 수 있다. 첫 실행에서 이 현상을 확인했으며 사용자는 그 상태에서도 화면과 버튼이 동작함을 확인했다. 진단 런타임은 절전 전 `Serial.end()`, 복귀 후 `Serial.begin()`으로 USB를 다시 열어 호스트의 재인식을 유도한다. 이것은 제품 RAM을 재부팅하는 동작이 아니다. 열린 시리얼 모니터는 포트를 다시 열어야 한다. [Espressif의 USB/light sleep 설명](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/api-guides/usb-serial-jtag-console.html#sleep-mode-considerations)

이 PC의 일반 `SerialPort.Open()`은 DTR/RTS 변경 과정에서 보드를 리셋할 수 있었다. 제공 도구는 `DeviceConsole.ps1`의 Win32 CDC 파일 핸들을 사용해 모뎀선을 건드리지 않는다. `tools/test-power.ps1 -Port COM5`에서 3회 연속 타이머 복귀 후 RAM의 요청 횟수 증가와 USB 재연결을 확인했다. 다른 시리얼 모니터가 리셋을 일으키면 이를 light sleep 자체의 상태 유지 실패와 구분한다.

제품 명령 `sleep 5`는 최대 5초간 자고 버튼으로 일찍 깨울 수 있다. 예약 시작·경고·마감이 더 가까우면 앞당긴다. v0.4의 무조작 기본값은 60초, 자동 수면은 최대 300초이며 시각 미확정이면 최대 30초다. `idle off`는 개발 중 자동 절전을 끄며 RAM에만 남는다. UX PDF의 무조작 10초와 차이는 [자료 대조](source-review-2026-10-03.md)에 기록했다. Wi-Fi/NTP 응답을 기다리는 중에는 자동 절전을 미뤄 한 차례 시도를 끝낸다.

## 매 기상 시각 동기화

[ClockSync.h](../firmware/RoutineDevice/src/application/ClockSync.h)는 연결/응답 대기·재시도와 시간 상태만 계산한다. [NetworkClock.cpp](../firmware/RoutineDevice/src/platform/esp32/NetworkClock.cpp)는 실제 Wi-Fi·NVS 설정·SNTP·시스템 시계에 연결한다. 루틴 엔진에 Wi-Fi 의존성은 없다.

- 부팅, 매 절전 복귀, 명시적 `sync` 요청에서 새 동기화 작업을 시작한다. 이미 연결됐어도 SNTP를 새로 시작한다.
- 저장된 단일 Wi-Fi에 재접속 → 새 NTP 응답 → 시스템 시계 보정 순서다. 접속과 응답 대기는 loop를 막는 반복문으로 구현하지 않는다.
- Wi-Fi 연결 15초, NTP 응답 15초가 지나면 재시도 대기로 전환한다. 30초 뒤 다시 시도한다. 이 값은 `ClockSync`에 모았다.
- 연결이 끊겨도 SSID/암호를 삭제하지 않는다. 정상 사용 중 공유기가 끊긴 경우에도 같은 설정으로 재접속하고 시각을 다시 맞춘다.
- `known`: 이번 부팅 이후 실제 NTP 응답을 한 번 이상 받음. `fresh`: 마지막 명시적 동기화 요청 이후 새 응답을 받음. 절전 뒤 이전 시각이 남아 있는 것은 새 동기화 성공이 아니다.
- 최초 동기화 전 화면은 `--:--`다. 성공 후 일시적으로 오프라인이 되면 RAM/RTC에 남아 진행하는 시각을 표시하되, 마지막 요청 성공 여부는 별도로 유지한다. 제품 도메인에 전달할 장기 오프라인 허용 시간·큰 보정/역행 처리 정책은 엔진 연결 시 확정한다.
- 시각 표시의 기본 시간대는 설정 구조체의 `KST-9`다. 내부 절대 시각은 UTC이며 API의 날짜 키/시간대를 이 표시 설정으로 대신하지 않는다.
- NTP 서버 설정도 `ClockSettings`로 교체한다. 기본은 `time.cloudflare.com`, 보조는 `pool.ntp.org`다. 연결되었다는 사실만으로 인터넷/NTP 접근 성공을 가정하지 않는다.
- SNTP 콜백에서는 atomic 수신 카운터만 증가시킨다. 화면/제품 상태는 loop에서 처리한다. 새 응답을 받으면 그 요청의 SNTP를 중지한다.

`status`는 설정 유무·연결·known/fresh·요청/성공 횟수와 마지막 절전 결과를 출력한다. SSID나 암호는 출력하지 않는다. 제품 화면은 Wi-Fi 단절과 배터리 미측정을 표시한다. 세부 상태는 이 명령으로 확인한다.

## 한 번만 하는 Wi-Fi 설정

v0.9.0에서 앱이 전달한 GATT 계약의 BLE 설정을 개발 빌드에 연결했다. [BLE 설정 계약](ble-wifi-provisioning.md)에 광고·수신 형식·개발 모드 제한을 정리했다. 일반 빌드와 USB 대체 경로는 다음 도구로 `NetworkClock::configure()`를 호출한다. BLE 설정 창이 열려 있다면 먼저 USB에서 `ble stop`으로 닫는다.

```powershell
.\tools\configure-wifi.ps1 -Port COM5
```

SSID와 암호는 PC에서 직접 입력한다. 암호는 숨김 입력을 사용하며 코드·명령행 인수·로그·PC 파일에 저장하지 않는다. USB 명령은 UTF-8 SSID와 암호를 16진수로 전달해 공백/한글의 구분 문제를 피한다. 이 인코딩은 암호화가 아니다. v0.9.0부터 후보는 RAM에서 시험하고 접속·IP 확보 후 기기의 `routine-wifi/good` NVS에 저장한다. 기존 SDK NVS 설정도 읽어 이관한다. 플래시 암호화를 구성했다는 뜻은 아니다.

지원 범위는 단일 개인용 Wi-Fi: SSID 1~32 UTF-8 바이트, 암호 없음 또는 8~63 ASCII 문자다. 기업용 인증·포털 로그인은 이 도구의 범위가 아니다. 새 설정은 최대 30초 시험하며 실패/취소 시 이전 성공 설정을 보존한다. USB의 시작 OK와 실제 접속·저장 완료는 구분하고, NTP 성공도 별도 상태다. 네트워크 자격 증명을 대화로 받을 필요가 없다.

BLE 개발 빌드에서는 D3을 5초 유지하면 설정을 열거나 닫는다. 중간에 확인이 실행되지 않도록 800ms 확인은 손을 뗄 때 전달한다. 설정 화면에서 정상 제품 입력과 자동 절전을 막으며 최대 5분 뒤 닫는다. 일반 빌드의 기존 800ms 확인 동작은 유지한다.

## 현재 연결 범위

기본 진입점은 v0.8 ProductRuntime이다. 입력·예약·완료/포기·먹이/성장/요약·저장·복원·화면을 조립한다. 진단 모드는 별도 빌드 매크로로 선택하며 현재 매핑에서는 Next=테마, Confirm=40ms 부저, Previous=80ms 부저다. 진단의 `time HH:MM` 등은 표시만 바꾸고 NTP 신뢰 상태를 바꾸지 않는다. 제품 모드는 가짜 시각 명령을 받지 않는다. 두 진입점 모두 버전이 일치하는 이미지 묶음을 사용한다.

영구 저장·예약 선택·먹이/성장/도감/하루 요약을 연결했다. 확보한 모션·API 어댑터·일정 갱신/보존도 연결했다. 실제 서버 계약과 일부 원본은 미확정이다. 실제 NTP 수신과 루틴 수행 중 정전 시험을 마친 상태는 아니다.

## 확인 근거

설치된 Arduino core 3.3.0-alpha1의 WiFi/STA/esp32-hal-time 소스와 Adafruit ST77xx 1.11.0의 sleep/display 명령 구현을 대조했다. 공식 참조: [Espressif Wi-Fi](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html), [ESP32-S3 sleep](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html), [시스템 시각/SNTP](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/system_time.html). 최신 온라인 문서와 설치 코어의 차이는 실제 보드 빌드로 확인한다.


v0.7에서 물리 제스처 수집은 5ms 주기의 별도 태스크, 논리 입력 전달은 16칸 큐로 분리했다. LCD 전송 중에도 수집하고, 큐가 넘치면 오래된 완료/확인 입력을 재생하지 않고 놓기부터 다시 기다린다. 절전 진입 때 태스크를 중지하고 복귀 때 큐·깨우기 입력을 재설정한다. 편지/먹기 연출 중 입력은 연출만 건너뛴다. 장치에 직접 연결한 추가 버튼은 아직 없으므로 세 버튼 배선 검증은 하지 않았다.


v0.8은 큐 항목에 표시 context를 추가한다. 다른 선택/단계/화면을 LCD에 보내는 동안 입력은 차단되고 전송 완료 뒤 새 context를 공개한다. 화면이 바뀌는 동안 누른 상태로 이어진 홀드는 새 화면의 확인으로 인정하지 않는다. 같은 화면의 시계 갱신이나 울기 프레임 갱신은 context를 유지한다. `status`의 Input context/discarded는 개발 진단용이다. 실물 USB 입력 테스트는 GPIO의 물리 제스처 테스트와 구분한다.

v0.8 실물 검사 중 절전에서 정상 복귀했어도 HWCDC의 상태 응답이 늦게 도착하는 경우가 있었다. `test-power.ps1`는 읽기 전용 status만 재조회하며 sleep 명령은 반복하지 않는다. 보완 후 3회 연속 복귀에서 요청 횟수 2→3→4→5, wake-cause=4, last-error=0과 진행 기록 유지를 확인했다. 상세 로그와 실제 NTP 수신 미검증 범위는 [검증 기록](../validation.md)을 따른다.
