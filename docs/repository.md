# 개발 환경 준비와 저장소 구성

2026-10-07 갱신. 소스·자산·문서를 다른 개발자가 이어서 다룰 수 있도록 정리한다. v0.8.2 실물 검증 이력과 v0.9.0 코드·빌드 검증을 구분하며, 펌웨어 구현 범위와 실제 연결 완료 여부는 [개발 상태](development.md)를 따른다.

## 보관 범위

| 경로 | 내용 |
|---|---|
| `firmware/RoutineDevice` | Arduino 진입점, 도메인/응용/표시/보드 계층, 생성된 자산 코드, ArduinoJson과 라이선스 |
| `tests`, `examples` | PC 회귀 검사, 구버전 저장 형식 fixture, 개발용 일정 예시 |
| `tools` | 빌드·검사·업로드·백업·자산 생성 스크립트와 라이브러리 버전/해시 |
| `assets` | 원본·변환 이미지, 배포용 `packed/ui.pak`, 글꼴과 OFL |
| `design` | Figma 출처/좌표/해시, 조회 문맥, 비교 화면, 당시 읽은 API 자료 |
| `docs` | 설계, 실행 계약, 결정 기록, 미확정 사항, 디자이너/서버·앱 인수인계 |
| `research`, 루트 문서 | 부품 자료, 배선 검토, 부품표, 검증 이력 |

`design/notion` 등 보관 자료는 조회 당시의 사본이다. 최신 명세로 자동 간주하지 않으며 [자료 대조](source-review-2026-10-03.md)와 사용자 결정을 함께 읽는다. 외부 PDF의 개발 환경·구조·개발 단계·인수 기준은 요구사항으로 채택하지 않았다.

`.local/`의 개인 설정·실기기 원장/전체 플래시 백업, `build/`의 실행 파일·빌드 캐시·원시 로그, `.tools/`의 다운로드 라이브러리, 임시 파일과 가상 환경은 Git에서 제외한다. 문서에 적힌 해당 경로는 검증한 PC의 보관 위치이며 새 checkout에 존재한다고 가정하지 않는다. 구버전 코덱 검사를 위한 `tests/fixtures/*.bin`과 배포용 `assets/packed/ui.pak`는 필요한 입력이므로 포함한다.

## Windows에서 빌드

확인한 환경은 Arduino CLI 1.2.0, ESP32 core **3.3.0-alpha1**, PowerShell 7, MSYS2 UCRT64의 g++이다. 임의의 최신 코어로 교체한 결과까지 검증한 것은 아니다. Python은 라이브러리 다운로드에 필요하며, 이미지/글꼴을 바꾸지 않는 빌드는 Node.js나 자산 재생성이 필요 없다.

1. Arduino CLI 또는 Arduino IDE의 CLI를 준비한다. CLI가 PATH에 없으면 아래 빌드 명령의 `-ArduinoCli`에 실행 파일 경로를 전달한다.
2. ESP32 개발용 보드 인덱스에서 확인한 코어 버전을 설치한다. 아래 명령은 `arduino-cli`가 PATH에 있는 경우다.

```powershell
$esp32Index = 'https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json'
arduino-cli core update-index --additional-urls $esp32Index
arduino-cli core install esp32:esp32@3.3.0-alpha1 --additional-urls $esp32Index
python tools/fetch_sources.py
```

`fetch_sources.py`는 고정 버전 Adafruit GFX 1.12.6, ST7789 1.11.0, BusIO 1.17.4를 `.tools/libraries`에 내려받는다. `tools/libraries.lock.json`에 원본 URL과 SHA-256을 보관한다. 이미 Git에 있는 Figma 자산은 다시 다운로드하지 않는다. 새 다운로드가 필요한 Figma URL은 만료될 수 있으므로 보관 원본을 유지한다.

저장소 루트에서 실행한다.

```powershell
.\tools\test.ps1 -Cxx 'C:/msys64/ucrt64/bin/g++.exe'
.\tools\build.ps1
# CLI가 PATH와 Arduino IDE 기본 위치에 없을 때
# .\tools\build.ps1 -ArduinoCli 'C:/path/to/arduino-cli.exe'
.\tools\verify-build.ps1
```

PC 검사는 14개 C++ 실행 파일과 PowerShell 백업 도구 검사를 포함한다. 일반 빌드는 `build/firmware`로 출력한다. 한글 경로에 대한 현재 링커 제한 때문에 중간 파일은 `%LOCALAPPDATA%/RoutineDeviceBuild/<경로 해시>`에 쓴다. `LOCALAPPDATA` 자체도 ASCII 경로여야 한다. 다른 OS에 대한 전체 도구 이식은 완료하지 않았다.

앱 BLE 개발 연동은 `tools/build.ps1 -BleDevelopment`, 결과 검증은 `tools/verify-build.ps1 -BleDevelopment`를 사용한다. 출력은 `build/firmware-ble-development`이며 별도 캐시를 쓴다. 이 빌드만 평문 BLE와 사설 IPv4 HTTP를 허용한다. [BLE 계약과 보드 사양](ble-wifi-provisioning.md)을 먼저 읽는다. `-Port`를 주지 않으면 업로드하지 않는다.

보드 옵션은 `tools/build.ps1`의 FQBN에 고정돼 있다: XIAO ESP32-S3 Plus, Flash 16MB, OPI PSRAM, `app3M_fat9M_16MB`, Hardware CDC. 임의로 파티션/전체 지우기 옵션을 바꾸지 않는다.

실물에 올릴 때는 연결된 포트를 확인해 지정한다. 다음 `COM5`는 이 PC에서 확인한 포트의 예시다.

```powershell
.\tools\build.ps1 -Port COM5
.\tools\install-assets.ps1 -Port COM5
.\tools\serial.ps1 -Port COM5 -Commands 'assets info','status'
```

기존 사용자 기록이 있는 기기는 먼저 `tools/backup-state.ps1 -Port COM5 -All`로 백업한다. 코드와 이미지 묶음의 버전은 함께 맞춘다. v0.8.2 요약 자산은 앱에 포함되어 직전 버전의 `dad8398c` 묶음은 그대로 사용할 수 있다. 자세한 설치/보존 계약은 [이미지 배포](image-assets.md)에 있다.

## 화면·자산을 수정할 때

기본 빌드에는 보관된 생성 파일을 사용한다. 재생성 환경에서 확인한 버전은 Python 3.12.14, Pillow 12.3.0, NumPy 2.3.5, Node.js 24.19.0, sharp 0.35.4다. 도구는 `CODEX_NODE_MODULES`를 지정하지 않으면 개발 PC의 런타임 경로를 사용하므로 다른 PC에서는 자신의 설치 경로를 명시한다.

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install Pillow==12.3.0 numpy==2.3.5
npm install --prefix .tools/node sharp@0.35.4
$env:CODEX_NODE_MODULES = (Resolve-Path '.tools/node/node_modules').Path

# 하루 요약의 소형 SVG/글꼴만 재생성하는 예
node tools/prepare_summary_assets.cjs
.\.venv\Scripts\python.exe tools/generate_summary_assets.py
.\tools\preview-summary.ps1 -Cxx 'C:/msys64/ucrt64/bin/g++.exe' -Python '.venv/Scripts/python.exe'
```

전체 묶음 생성 순서는 [이미지 배포 문서](image-assets.md)를 따른다. 요약용 생성과 FAT 묶음 생성을 혼동하지 않는다. 생성 결과와 manifest를 함께 검토하고 관련 검사·빌드를 거친다. Figma 전체 화면 캡처는 비교 자료이며 펌웨어 이미지로 사용하지 않는다.

## 기준 버전과 후속 작업

v0.8.2의 PC 검사, ESP32 빌드/업로드, RAM 데모 및 요약 전환 검사는 통과했다. [검증 이력](../validation.md)에 측정 범위와 한계를 기록했다. 저장소를 정리하면서 펌웨어 소스나 실기기 데이터를 바꾸지 않았다.

| 배포물 | 크기 | SHA-256 |
|---|---:|---|
| 검증한 앱 바이너리 | 2,765,984 B | `c2e8e6d8c0b72e4f3e46a93152af5f8d91a5bbdc69d1c523567c22af25420f3e` |
| `assets/packed/ui.pak` | 7,182,088 B | `cd2d255aa1f769a21264bd56ded587c2752ca06adaf0140a70c6ebf52288f4a7` |

앱 바이너리는 로컬 검증 기준이며 Git에 포함하지 않는다. 다른 컴파일 환경에서 동일 바이너리 해시를 보장하지 않으며 `verify-build.ps1`는 해당 빌드의 소스/캐시/배포물 일치 여부를 확인한다.

실제 서버 접속 정보·Wi-Fi·등록 코드와 미확정 API 계약, 누락 디자인 자산, 실제 NTP/전류/전원 차단 실측은 후속 작업이다. [디자이너 전달 문서](designer-handoff.md)와 [서버·앱 전달 문서](server-app-handoff.md)를 각각 사용한다. 인증 정보는 코드에 쓰지 않고 제공된 USB 설정 도구로 전달한다. Figma·팀 문서·이미지의 프로젝트 권리는 그대로 유지하며, 외부 글꼴과 ArduinoJson의 라이선스도 함께 보관한다.
