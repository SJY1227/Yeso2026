# 이미지 묶음과 펌웨어 배포

Product v0.6부터 이미지는 기존 FAT 데이터 파티션의 **버전별 파일**로 저장하고, 코드에는 좌표·크기·오프셋과 기대 버전만 둔다. 10종을 추가하면서 앱 공간에 이미지를 계속 붙이는 한계를 해결하기 위한 분리다. 파티션 배치는 유지한다. 이미지 설치 자체는 진행 상태를 변경하지 않는다; v0.7 상태 스키마 4는 별도의 동기화 변경이다.

## 소스부터 화면까지

1. `design/theme-assets.json` / `design/catalog-assets.json`: Figma 노드, 원본 URL, 파일, SHA-256. 원본은 `assets/source`에 보관.
2. `tools/prepare_images.cjs`, `prepare_product_images.cjs`, `prepare_catalog_images.cjs`, `prepare_themes.cjs`: 원본 레이어의 좌표·크롭·회전을 LCD 좌표로 변환. `design/references`의 화면 캡처는 입력으로 쓰지 않음.
3. `tools/generate_asset_pack.py`: 모든 이미지 생성기를 같은 묶음으로 실행. `assets/packed/ui.pak`, `manifest.json`, `src/generated/*`를 함께 생성. 이미지 타입은 RGB565+8비트 알파/4KiB LZSS. 글꼴은 오류 안내를 위해 코드에 유지.
4. `ImageArchive`: 헤더·크기·CRC를 기대 버전과 검증하고 바이트 제공자에 연결. `ImageRenderer`는 검증된 바이트만 읽으며 파일 시스템을 모름.
5. `AssetStorage`: FAT 파일 읽기, PSRAM 소유, USB 설치. `ProductController`/상태 저장에는 의존하지 않음.

전체 생성은 `prepare_*` 이후 `generate_asset_pack.py`로 한다. 개별 `generate_*_assets.py` 실행만으로 배포용 생성 파일을 덮어쓴 상태는 사용하지 않는다. 그림이나 좌표 변경 뒤에는 **코드와 묶음을 함께 재생성·검증·배포**한다.

```powershell
& tools/test.ps1
& tools/build.ps1 -Port COM5
& tools/install-assets.ps1 -Port COM5
& tools/serial.ps1 -Port COM5 -Commands 'assets info','status'
```

묶음 생성기가 실행된 경로와 무관하게 테스트는 작업공간 루트에서 `assets/packed/ui.pak`를 읽는다. 미리보기 생성 후 `tools/verify_theme_assets.py`는 묶음의 모든 이미지와 변환 PNG를 독립 복호화하여 비교하고 검토용 시트를 만든다.

## 파일 형식과 설치 보장

16바이트 헤더는 ASCII `RDAS0001` 8바이트, payload 길이 LE32, payload CRC32 LE32다. 뒤에는 압축 이미지가 순서대로 이어진다. 각 C++ 이미지 descriptor는 payload 내 범위를 갖는다. 펌웨어의 `AssetPack.h`에 전체 길이·CRC·`/ffat/ui-<crc>.pak` 경로를 고정한다. CRC는 전송/저장 손상 검출이며 원격 인증이나 서명 검증을 대신하지 않는다.

부팅 시 4KiB씩 읽어 전체 CRC를 검증한다. PSRAM에는 최대 압축 그림 두 개(2 × 103,644 = 207,288바이트)만 캐시한다. 렌더 중 캐시 미스는 파일 읽기를 수행하지만 복원기는 동적 할당을 하지 않는다. 묶음 전체 크기만큼 RAM을 요구하지 않는다. 유효하지 않으면 내장 글꼴로 ‘화면 자료를 확인해 주세요’를 표시하고 제품 입력은 막지만 USB 설치는 허용한다. 사용자 진행 기록을 포맷하거나 초기화해서 해결하지 않는다.

USB 설치는 `assets begin <bytes> <crc>` → `assets chunk <offset> <length> <chunk-crc>`와 해당 길이의 바이너리 → `assets commit`이다. 최대 청크는 4096바이트, 매 청크에 수신 준비/완료 ACK가 있다. 30초 무진전 시 임시 파일을 버린다. 시작 크기·버전·청크 오프셋·길이·CRC를 검사하고 전체 파일을 flush/fsync/close한 뒤 다시 읽어 검증한다. 검증된 파일만 버전 경로로 옮기고 파일 제공자를 활성화한다. 설치 도중에는 제품 입력/절전을 미룬다.

`UsbConsole.h`는 부팅과 절전 복귀 시 HWCDC 수신 큐를 8192바이트로 준비한다. 코어 기본값 256바이트는 LCD 전송 중 4KiB 청크를 잃을 수 있다. 청크 수신 명령마다 화면을 다시 그리지 않으며, 설치로 이미지 준비 상태가 달라졌을 때만 다시 그린다. 첫 설치 시험에서 발견한 실제 수신 누락을 반영한 경계다.

Windows 설치 도구는 4KiB 청크 헤더와 본문을 8KiB 수신 큐 안에 함께 보낸 뒤 ACK를 확인한다. HWCDC 응답이 지연될 때는 읽기 전용 `assets info`로 현재 설치 세션에서 검증해 수신한 오프셋을 조회한다. 같은 청크나 제품 입력을 반복 실행하지 않는다. 이 조회는 본문을 전부 보낸 뒤에만 수행한다. 본문을 아직 보내지 않은 상태에서 READY 응답을 기다리며 명령을 섞지 않는다. 청크 ACK는 임시 수신 확인이며, 영속 설치 성공은 전체 파일을 다시 검증한 commit으로 확정한다.

PC 콘솔은 DTR/RTS를 바꾸지 않고 최대 20ms의 짧은 수신 대기를 사용한다. MAXDWORD와 제한된 상수 시간을 조합한 동작은 [Microsoft COMMTIMEOUTS 문서](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts)에 따른다.

중단된 설치는 `ui-upload.tmp`에만 남는다. `state-a.bin`, `state-b.bin`, NVS generation은 설치 명령이 건드리지 않는다. 이전 버전의 유효한 묶음도 남겨 두어 해당 펌웨어를 복원할 수 있게 한다. 현재 버전의 이미 검증된 묶음이 있으면 설치 도구는 건너뛴다. ACK 유실을 입력 재전송 성공으로 추정하지 말고 `assets info`로 수신 상태를 확인한다.

묶음 보관 자체가 제품 상태 다운그레이드를 지원한다는 뜻은 아니다. 새 종을 획득한 뒤 두 종만 아는 옛 펌웨어로 되돌리면 콘텐츠 검증에서 거절될 수 있다. 코드 복원 전에는 상태 스키마뿐 아니라 소유한 종 ID도 호환되는지 확인한다.

파일 시스템 자체가 손상된 경우나 실제 전원 차단의 모든 시점을 검증했다고 주장하지 않는다. 자동 FAT 포맷은 기존의 ‘전 파티션이 비어 있고 저장 세대가 없음’ 조건에만 적용한다. 향후 여러 버전이 쌓여 공간이 모자라면 보존할 펌웨어/묶음 버전을 확인한 뒤 전용 정리 도구를 추가한다. 사용자 기록을 지우거나 검증하지 않은 파일을 활성화하지 않는다.

## 캐릭터 확장

`design/character-registry.json`과 생성 도구에 고정 종 ID, 세 단계 원본, 테마를 연결한다. 도감 페이지 순서와 저장 ID는 분리한다. 신규 단계에 원본 잠금 그림이 없으면 `nullptr`로 명시하고 `???` 대체 화면을 사용한다. 원본 미확보 상태는 [디자이너 문서](designer-handoff.md)에 기록한다.

현재 10종·30단계, 잠금 14개, 먹이 31종, 홈/테마/UI를 합쳐 395개 이미지다. 미확보 잠금 16개에 가짜 자산을 생성하지 않았다. 펌웨어 변경 없이 온라인으로 임의의 새 콘텐츠를 받는 CDN/OTA 체계를 구현한 것은 아니다.

기존 `ROUTINE_HARDWARE_CHECK` 진입점도 같은 묶음을 읽고 설치 명령을 지원한다. 진단 진입점은 FAT를 포맷하거나 제품 원장을 만들지 않는다. 완전히 빈 기기는 기본 Product 실행에서 기존 초기화 조건을 충족해 파일 시스템을 준비한 후 진단 모드로 전환한다.


## v0.7 재현과 용량

`extended-assets.json`, `motion-assets.json`, `letter-assets.json`, `bite-assets.json`에 추가 원본/해시를 보관했다. `prepare_extended_images.cjs`, `prepare_motions.py`도 생성 경로에 포함한다. 최종 `generate_asset_pack.py`가 모든 descriptor와 묶음을 함께 생성한다. `verify_theme_assets.py --pack-only`는 미리보기 없이 모든 항목을 독립 복호화해 PNG와 비교한다. `preview-scenarios.ps1`은 같은 C++ 렌더러로 단계/테마/먹이/모션 검토 이미지를 만든다.

현재 묶음은 7,182,088바이트, 395개, payload CRC dad8398c다. 한글 전체 글꼴은 2비트/4단계 농도로 1,188,324바이트이며 오류 화면에도 필요하므로 앱에 둔다. 화면 버퍼는 153,600바이트다. 현재 FAT 크기에서 7MiB급 묶음 두 개를 동시에 보관할 여유는 없으므로 다음 대규모 교체 때 저장 공간과 기존 묶음 백업/정리 전략을 확인해야 한다. 진행 기록을 지워서 설치 공간을 만들지 않는다. 과거 회차 백업도 같은 FAT 공간을 사용한다.

21개 울기 몸체와 눈물은 현재 export에 불투명 흰 배경이 포함돼 있다. 이것을 투명한 최종 자산이라고 설명하지 않는다. 먹기 53프레임은 Figma 호출 한도로 미추출이며 원본 미제작으로 분류하지 않는다. 자세한 노드/요청은 디자이너 전달 문서에 있다.

## v0.8.2 소형 요약 자산

`design/summary-assets.json`에 12개 조회 프레임의 원본 SVG URL·파일·해시를 저장했다. `tools/prepare_summary_assets.cjs` → `tools/generate_summary_assets.py`로 `SummaryAssets.*`를 재생성한다. 별도로 생성하는 **64,421바이트 압축 SVG 래스터 29개**와 고정 요약 글꼴은 이번 버전에서 앱에 포함한다. 기존 FAT 묶음/이미지 descriptor는 변경하지 않으므로 이미지 재설치가 필요 없다. `ImageRenderer`의 기존 내장 바이트 지원을 이용하고 캐시/저장 형식을 추가하지 않는다. 앞으로 큰 캐릭터 그림을 이 경로에 누적하는 용도로 쓰지 않는다.

요약 준비 도구는 캡처가 아니라 실제 SVG 레이어를 배치한다. 도입/마무리 말풍선은 원본 형태를 유지해 기존 10개 panel 토큰으로 채운다. 원본 체크 3개, 방향 화살표, 미완료 장식과 봉투 앞뒤 레이어를 보관했다. `summary_tests.cpp`의 같은 C++ 렌더러 출력과 `design/references/summary`의 원본 캡처를 비교했다. 조회한 요약 연속 프레임에는 native keyframe timing이 없어 속도는 별도 조정값이다.
