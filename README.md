# 루틴 기기 펌웨어

XIAO ESP32-S3 Plus + DFRobot DFR0664(ST7789, 240×320). 제품 규칙, 화면 조작, 저장, 보드 입출력을 나누어 같은 C++ 코드를 PC와 보드에서 검증한다. [설계 문서](docs/README.md), [실행 구조와 확장 지점](docs/product-runtime.md), [검증 기록](validation.md)을 함께 본다.

저장소를 처음 받은 개발자는 [개발 환경 준비와 저장소 구성](docs/repository.md)부터 읽는다. 펌웨어·이미지 묶음·생성 도구·설계/인수인계 문서를 함께 보관하며, 개인 설정과 실물 기기의 진행 백업은 Git에 포함하지 않는다.

기본 실행은 **Product v0.9.0**다. 예약 → 편지 → 작은 루틴 → 완료/포기 확인 → 먹이 획득·소비 → 성장·진화 → 하루 요약과 재부팅 복원을 연결했다. 사용자의 Figma 우선 결정에 따라 단계 목표는 40·50·60(누적 40·90·150)이다. 저장 실패·손상·미지원 스키마에서는 성공으로 진행하거나 초기화하지 않는다. API 명세는 확보했고 [UX·Figma와의 충돌 및 연동 과제](docs/source-review-2026-10-03.md)를 기록했다. API 통신·일정 갱신/보존 코드는 구현했다. 실제 서버 연결과 미확정 계약·미추출 원본은 남아 있다.

v0.9.0은 [앱 BLE 설정 계약](docs/ble-wifi-provisioning.md)을 개발 빌드에 추가하고, Wi-Fi 성공 후 저장과 글꼴 무손실 압축을 반영했다. `tools/build.ps1 -BleDevelopment`에서만 평문 BLE·로컬 HTTP를 허용한다. D3 5초 유지로 설정을 열고, 이 빌드의 0.8초 확인은 손을 뗄 때 실행한다. 이번 변경은 코드·PC/빌드 검증 범위이며 실제 앱·보드 연결은 미검증이다.

공모전 서버 연동에서는 사용자 지시대로 `battery: 100`을 고정 전송한다. 배터리 미측정으로 루틴 sync가 중단되지 않으며, 실제 측정값은 아니다. 기기 화면의 배터리 미측정 표시는 유지한다.

## 버튼

| 현재 D3 한 개 | 의도 | 루틴 화면 |
|---|---|---|
| 한 번 짧게 | Next | 완료 선택 |
| 두 번 짧게 | Previous | 포기 선택 |
| 0.8초 유지 | Confirm | 선택한 항목의 확인창 열기 |

포기 확인창은 취소를 기본 선택한다. 이전/다음으로 선택한 뒤 확인해야 실제로 포기한다. 완료 확인창에서 이전/다음은 취소, 확인은 완료 확정이다. 깨우는 입력은 완료나 포기로 재사용하지 않는다. 세 버튼을 연결할 때는 `Intent.h`의 매핑과 `Board`의 핀 입력만 교체한다. 엔진과 화면 처리에는 물리 버튼 번호를 전달하지 않는다.

홈에서 이전/다음으로 도감을 연다. 도감 안에서는 이전/다음으로 항목을 탐색하고 확인으로 선택한다. 맨 끝은 홈으로 돌아가는 항목이다. 10종의 각 3단계, 총 30개를 별도 카드로 보여주며, 수집한 단계는 다시 키워도 남는다. 먹이 31종의 원본 그림·이름·획득/남은 수를 연결했다. 사용자 확정에 따라 `/30`을 사용한다. 잠금 원본은 14개를 확보했고 나머지 16개는 물음표로 표시한다. 자세한 조작과 한계는 [실행 계약](docs/product-runtime.md)에 명시했다.

## 배선

| 신호 | 보드 | GPIO |
|---|---|---:|
| LCD SCLK | D8 | 7 |
| LCD MOSI | D10 | 9 |
| LCD CS | D2 | 3 |
| LCD RES | D1 | 2 |
| LCD DC | D0 | 1 |
| 버튼 | D3 | 4 |
| 부저 트랜지스터 제어 | D11 | 38 |

BL/SC는 사용하지 않는다. 버튼 LOW 눌림·내부 풀업, 부저 HIGH 활성, 화면 세로·SPI 20MHz다. [HardwareConfig.h](firmware/RoutineDevice/src/platform/esp32/HardwareConfig.h)에 정의를 모았다. 별도 BL 배선을 요구하지 않고 LCD/MCU light sleep을 사용한다.

## 빌드와 PC 검증

```powershell
.\tools\test.ps1
.\tools\build.ps1
# 확인한 USB 포트에만 업로드
.\tools\build.ps1 -Port COM5
.\tools\install-assets.ps1 -Port COM5
```

Arduino CLI 1.2.0, 설치된 ESP32 core 3.3.0-alpha1, Flash 16MB/OPI PSRAM/Hardware CDC 설정을 사용한다. 기존 파티션 배치를 유지한다. Adafruit GFX 1.12.6, ST7789 1.11.0, BusIO 1.17.4는 `.tools/libraries`에 고정했다. 다른 PC는 `tools/fetch_sources.py`로 준비하고 경로를 지정한다. 한글 경로 문제를 피하기 위해 `%LOCALAPPDATA%/RoutineDeviceBuild/`를 중간 빌드 경로로 쓴다.

14개 PC 테스트 실행 파일이 BLE 프레이밍·복구·설정 입력, 글꼴 11,361자 원본 일치, 기존 입력/렌더링·제품 저장·도감/이미지 복원을 검사한다. 제품·도감 테스트는 플랫폼을 제외한 휴대 가능 모듈의 `.cpp`를 자동 수집한다. 화면별 입력은 `ProductInput.cpp`, 상태 확정은 `ProductController.cpp`, 글꼴/픽셀 처리는 `Drawing.cpp`에서 수정한다. 실제 공유기·NTP·정전·전류 검증을 PC 테스트로 대신하지 않는다. `ROUTINE_HARDWARE_CHECK` 매크로를 정의하면 별도 진단 진입점을 선택한다. 검증 바이너리와 최초 전체 플래시 백업은 `.local/backups/`에 보관했다.

## Wi-Fi, 일정, 절전

```powershell
.\tools\configure-wifi.ps1 -Port COM5
.\tools\import-schedule.ps1 -Port COM5 -Path '내-일정.json'
.\tools\serial.ps1 -Port COM5 -Commands 'status','idle off'
```

Wi-Fi 암호는 PC에서 숨김 입력으로 받는다. 처음 저장한 네트워크를 재사용하며 매 절전 복귀마다 새 시각 동기화를 요청한다. 최초 NTP 응답 전에는 `--:--`를 표시하고 시각에 의존하는 처리를 보류한다.

`examples/schedule.json`은 형식을 보여주는 2030년 예시다. 실제 날짜·시각으로 별도 작성한다. USB 형식은 개발 도구이며 서버 API가 아니다. 현재 한도는 16회차 × 16단계, 문자열당 UTF-8 95바이트다. 같은 일정을 다시 넣는 것은 안전하지만 USB로 다른 버전을 덮어쓰는 것은 거절한다. API의 미래 일정 갱신/ACK/과거 보존은 별도 어댑터에서 처리한다.

기본 무조작 절전은 60초다. `idle off`로 개발 중 자동 절전을 끌 수 있다. `sleep 2`는 2초 타이머 절전을 요청하며 예약 경계가 더 가까우면 앞당긴다. 매 기상 Wi-Fi/NTP 요청과 USB 재연결은 공통 절전 함수에 모았다. 부팅 후 무조작 설정은 기본값으로 돌아간다.

USB 명령: `help`, `status`, `sync`, `idle off`, `idle 1..3600`, `sleep 1..86400`, `input previous`, `input next`, `input confirm`, `reboot`. 제품에서는 가짜 시각/성장 명령을 받지 않는다. Windows 도구는 모뎀선을 변경하지 않아 불필요한 USB 리셋을 피한다.

## 디자인과 범위

Figma는 읽기만 한다. 원본 개별 자산은 `assets/source/`, 문맥은 `design/context/`, 자산 목록은 `design/routine-assets.json`에 있다. `prepare_*` 도구가 레이어를 배치하고 `generate_asset_pack.py`가 이미지 묶음과 C++ 좌표/글꼴을 함께 만든다. [이미지 배포 계약](docs/image-assets.md)을 따른다. 비교용 전체 스크린샷을 펌웨어 자산으로 사용하지 않는다. `design/previews/product-*.png`는 동일 C++ 렌더러의 PC 출력이다.

도감은 `presentation/CatalogRenderer.*`, 도감용 자산/글꼴은 `tools/prepare_catalog_images.cjs` → `tools/generate_asset_pack.py`에서 재생성한다. 2026-10-04 Figma MCP로 45개 프레임의 원본을 직접 읽고 성장 단계·먹이 31종·화살표 SVG·정확한 배치와 카드/버튼을 연결했다. 출처와 SHA-256은 `design/catalog-assets.json`, 좌표는 `design/catalog-geometry.json`에 있다. [원본 대조 이미지](design/previews/catalog-direct/figma-comparison.png)와 [디자인 참조](design/README.md)를 함께 본다.

현대 한글 11,172음절·호환 자모·ASCII를 지원한다. 동적 한글은 4단계 농도 비트맵, 고정 선택 문구·시계·작은 수치는 표시 크기별 안티앨리어싱 글꼴을 쓴다. 긴 문구는 축소·줄바꿈하고 한자/이모지는 `?`로 표시한다. Wi-Fi 단절과 배터리 미측정을 실제 상태에 따라 표시한다. 30단계 홈·31종 먹이·테마·확보한 모션을 연결했고 잠금 원본 16개와 일부 모션/투명 export는 미확보다. 활성 256단계와 백업 후 과거 기록 정리를 지원하지만 서버 계약과 유한한 저장 공간을 전제로 한다.

팀원 PDF의 개발 환경·펌웨어 구조·개발 단계·통합 테스트·인수 기준은 사용자 요청에 따라 요구사항으로 채택하지 않았다.

팀 전달 문서: [디자이너 — 자산·이름·화면](docs/designer-handoff.md), [서버·앱 개발자 — API·데이터 책임](docs/server-app-handoff.md).


v0.7은 HTTPS claim/sync 어댑터, 안전한 일정 병합과 과거 기록 보관, 스키마 4, USB 진행 백업, 단계별 홈/테마/먹이, 편지·울기·먹기 연출을 포함한다. 실제 서버 연결에는 접속 정보·등록 코드가 필요하며, 공모전에서는 배터리 100 고정값으로 동기화를 진행한다. 현재 상태와 자료 의존성은 [개발 상태](docs/development.md), 원본/모션 한계는 [디자이너 문서](docs/designer-handoff.md), API 계약은 [서버·앱 문서](docs/server-app-handoff.md)를 따른다. `tools/backup-state.ps1 -Port COM5`로 진행 백업을 만들 수 있고 `motion off/on`으로 연출 설정을 저장한다.


v0.8은 동기화 중 화면 유지, 표시 context에 묶인 버튼 입력, API 본문 검사, 불변 과거 파일과 `backup-state.ps1 -All`을 보완한다. 제품 성장 규칙·스키마 4·이미지 묶음은 그대로다.

루틴 실물 체험은 [RAM 전용 테스트 모드](docs/device-demo.md)를 사용한다. USB로 시작하고 종료/재부팅하면 원래 기록으로 돌아간다.

하루 요약은 Figma의 소개 → 편지 열기 → 루틴별 결과 → 마무리 인사를 따른다. D3 한 번/두 번으로 요약 장을 넘기고 길게 눌러 나간다. [원본과 비교](design/previews/summary/comparison.png). `tools/test-demo.ps1 -Port COM5 -LeaveSummary`는 실제 원장을 보존한 RAM 시험을 마친 뒤 요약 소개를 남긴다.
