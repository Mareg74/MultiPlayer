# Build + package MultiPlayer for Windows release (branch: windows).
$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$cmakeTxt = Get-Content -Raw "CMakeLists.txt"
if ($cmakeTxt -match 'project\(MultiPlayer VERSION ([0-9.]+)') {
    $Version = $Matches[1]
} else {
    $Version = '0.1.0'
}
if ($env:MP_VERSION) { $Version = $env:MP_VERSION }

$BuildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $Root 'build-release' }
$OutDir = Join-Path $Root 'dist\windows'
$Zip = Join-Path $OutDir "MultiPlayer-$Version-Windows.zip"

$QtDir = $env:CMAKE_PREFIX_PATH
if (-not $QtDir) { $QtDir = $env:QT_ROOT_DIR }
if (-not $QtDir) { $QtDir = $env:Qt6_DIR }
if ($QtDir -and ($QtDir -match '[/\\]lib[/\\]cmake[/\\]Qt6$')) {
    $QtDir = Split-Path (Split-Path (Split-Path $QtDir -Parent) -Parent) -Parent
}
if (-not $QtDir) { $QtDir = 'C:\Qt\6.8.3\msvc2022_64' }
$WinDeployProbe = Join-Path $QtDir 'bin\windeployqt.exe'
if (-not (Test-Path $WinDeployProbe)) {
    $found = Get-ChildItem -Path $QtDir -Recurse -Filter windeployqt.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $QtDir = Split-Path (Split-Path $found.FullName -Parent) -Parent }
}
$FfmpegRoot = if ($env:FFMPEG_ROOT) { $env:FFMPEG_ROOT } else { 'C:\deps\ffmpeg' }
$RtMidi = if ($env:RTMIDI_SOURCE_DIR) { $env:RTMIDI_SOURCE_DIR } else { 'C:\deps\rtmidi' }

Write-Host "==> Configure $Version (Qt=$QtDir)"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Push-Location $BuildDir
try {
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release `
      "-DCMAKE_PREFIX_PATH=$QtDir" `
      "-DFFMPEG_ROOT=$FfmpegRoot" `
      "-DRTMIDI_SOURCE_DIR=$RtMidi" `
      $Root
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

    Write-Host '==> Build'
    cmake --build . --parallel
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
} finally {
    Pop-Location
}

$Exe = Join-Path $BuildDir 'MultiPlayer.exe'
if (-not (Test-Path $Exe)) { throw "Missing $Exe" }

Write-Host '==> Stage pack folder'
$Stage = Join-Path $BuildDir 'pack'
if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
Copy-Item $Exe $Stage

# FFmpeg DLLs
Get-ChildItem (Join-Path $FfmpegRoot 'bin\*.dll') -ErrorAction SilentlyContinue | Copy-Item -Destination $Stage -Force

# NDI
$ndiCandidates = @(
    (Join-Path $Root 'third_party\NDI\runtime\Processing.NDI.Lib.x64.dll'),
    'C:\Program Files\NDI\NDI 6 Runtime\v6\Processing.NDI.Lib.x64.dll',
    'C:\Program Files (x86)\NDI\NDI 6 Runtime\v6\Processing.NDI.Lib.x64.dll'
)
if ($env:NDI_RUNTIME_DIR_V6) {
    $ndiCandidates += (Join-Path $env:NDI_RUNTIME_DIR_V6 'Processing.NDI.Lib.x64.dll')
}
$ndiOk = $false
foreach ($c in $ndiCandidates) {
    if ($c -and (Test-Path $c)) {
        Copy-Item $c (Join-Path $Stage 'Processing.NDI.Lib.x64.dll') -Force
        Write-Host "NDI: $c"
        $ndiOk = $true
        break
    }
}
if (-not $ndiOk) { Write-Warning 'NDI DLL not found — pack without NDI runtime' }

# Spout
$spoutCandidates = @(
    (Join-Path $Root 'third_party\Spout\bin\SpoutLibrary.dll'),
    (Join-Path $Root 'third_party\Spout\lib\SpoutLibrary.dll')
)
$spoutOk = $false
foreach ($c in $spoutCandidates) {
    if (Test-Path $c) {
        Copy-Item $c (Join-Path $Stage 'SpoutLibrary.dll') -Force
        Write-Host "Spout: $c"
        $spoutOk = $true
        break
    }
}
if (-not $spoutOk) { Write-Warning 'SpoutLibrary.dll not found' }

$WinDeploy = Join-Path $QtDir 'bin\windeployqt.exe'
if (-not (Test-Path $WinDeploy)) { throw "windeployqt not found: $WinDeploy" }
Write-Host '==> windeployqt'
& $WinDeploy --release --no-translations (Join-Path $Stage 'MultiPlayer.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path $Zip) { Remove-Item $Zip -Force }

Write-Host "==> Zip $Zip"
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath $Zip -Force

Write-Host "PACK_OK $Zip"
Get-Item $Zip | Format-List FullName, Length
