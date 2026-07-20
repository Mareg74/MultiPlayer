$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path 'C:\deps' | Out-Null

$ffmpegUrl = 'https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip'
$ffmpegZip = 'C:\deps\ffmpeg.zip'
Write-Host 'Downloading FFmpeg...'
Invoke-WebRequest -Uri $ffmpegUrl -OutFile $ffmpegZip -UseBasicParsing
Write-Host 'Extracting FFmpeg...'
if (Test-Path 'C:\deps\ffmpeg-tmp') { Remove-Item 'C:\deps\ffmpeg-tmp' -Recurse -Force }
Expand-Archive -Path $ffmpegZip -DestinationPath 'C:\deps\ffmpeg-tmp' -Force
$root = Get-ChildItem 'C:\deps\ffmpeg-tmp' -Directory | Select-Object -First 1
if (Test-Path 'C:\deps\ffmpeg') { Remove-Item 'C:\deps\ffmpeg' -Recurse -Force }
Move-Item $root.FullName 'C:\deps\ffmpeg'
Remove-Item 'C:\deps\ffmpeg-tmp' -Recurse -Force
Remove-Item $ffmpegZip -Force
Get-ChildItem 'C:\deps\ffmpeg' | ForEach-Object { $_.Name }
Write-Host 'FFMPEG_OK'

# RtMidi: clone single-header-ish sources via git tag
$rtmidiDir = 'C:\deps\rtmidi'
if (Test-Path $rtmidiDir) { Remove-Item $rtmidiDir -Recurse -Force }
Write-Host 'Cloning RtMidi...'
& 'C:\Program Files\Git\bin\git.exe' clone --depth 1 --branch 6.0.0 https://github.com/thestk/rtmidi.git $rtmidiDir
Write-Host 'RTMIDI_OK'
