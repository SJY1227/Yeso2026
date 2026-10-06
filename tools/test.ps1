param([string]$Cxx = 'C:/msys64/ucrt64/bin/g++.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force -Path (Join-Path $root 'build') | Out-Null
Push-Location -LiteralPath $root
try {
    # MinGW's linker cannot resolve this machine's Korean absolute path.
    # Compile every portable module so extracting a .cpp cannot silently omit it
    # from the PC build. Hardware sources remain exclusive to Arduino; the
    # portable file-archive primitive has its own real-filesystem test below.
    $portableSources = @(
        foreach ($area in @('application','content','domain','presentation','generated')) {
            $directory = 'firmware/RoutineDevice/src/' + $area
            Get-ChildItem -LiteralPath $directory -Filter '*.cpp' -File | Sort-Object Name | ForEach-Object {
                $directory + '/' + $_.Name
            }
        }
    )
    $cases = @(
        @{Name='provisioning-tests'; Sources=@('tests/provisioning_tests.cpp','firmware/RoutineDevice/src/application/WifiProvisioning.cpp')},
        @{Name='font-bitmap-tests'; Sources=@('tests/font_bitmap_tests.cpp') + $portableSources},
        @{Name='native-tests'; Sources=@('tests/native_tests.cpp') + $portableSources},
        @{Name='diagnostics-tests'; Sources=@('tests/diagnostics_tests.cpp','firmware/RoutineDevice/src/diagnostics/HardwareCheck.cpp')},
        @{Name='routine-engine-tests'; Sources=@('tests/routine_engine_tests.cpp','firmware/RoutineDevice/src/domain/RoutineEngine.cpp')},
        @{Name='device-services-tests'; Sources=@('tests/device_services_tests.cpp')},
        @{Name='product-tests'; Sources=@('tests/product_tests.cpp') + $portableSources},
        @{Name='summary-tests'; Sources=@('tests/summary_tests.cpp') + $portableSources},
        @{Name='catalog-tests'; Sources=@('tests/catalog_tests.cpp') + $portableSources},
        @{Name='asset-archive-tests'; Sources=@('tests/asset_archive_tests.cpp','firmware/RoutineDevice/src/presentation/ImageArchive.cpp')},
        @{Name='api-tests'; Sources=@('tests/api_tests.cpp') + $portableSources},
        @{Name='motion-tests'; Sources=@('tests/motion_tests.cpp','firmware/RoutineDevice/src/presentation/Motion.cpp','firmware/RoutineDevice/src/generated/MotionAssets.cpp')},
        @{Name='file-archive-tests'; Sources=@('tests/file_archive_tests.cpp','firmware/RoutineDevice/src/platform/FileArchive.cpp')},
        @{Name='demo-tests'; Sources=@('tests/demo_tests.cpp','firmware/RoutineDevice/src/diagnostics/DemoSession.cpp') + $portableSources}
    )
    foreach ($case in $cases) {
        $binary = 'build/' + $case.Name + '.exe'
        & $Cxx -std=c++17 -O2 -static -Wall -Wextra -Werror -o $binary $case.Sources
        if ($LASTEXITCODE -ne 0) { throw "Test compile failed: $($case.Name)" }
        & (Join-Path $root $binary)
        if ($LASTEXITCODE -ne 0) { throw "Tests failed: $($case.Name)" }
    }
    # Isolate the TEST-only console stub from any later real device commands.
    & (Get-Process -Id $PID).Path -NoProfile -NonInteractive -File (Join-Path $root 'tests/backup_tool_tests.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Backup tool tests failed' }
} finally { Pop-Location }
