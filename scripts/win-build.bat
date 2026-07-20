@echo off
setlocal EnableExtensions
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=arm64
if errorlevel 1 exit /b 1

set "SRC=\\Mac\Home\Documents\CURSOR WORKSPACES\MultiPlayer"
set "BUILD=C:\build\MultiPlayer"
set "QTDIR=C:\Qt\6.8.3\msvc2022_64"
set "FFMPEG_ROOT=C:\deps\ffmpeg"
set "RTMIDI_SOURCE_DIR=C:\deps\rtmidi"
set "PATH=%QTDIR%\bin;C:\Program Files\CMake\bin;C:\Windows\System32\config\systemprofile\AppData\Local\Microsoft\WinGet\Links;%PATH%"

if not exist "%BUILD%" mkdir "%BUILD%"
cd /d "%BUILD%"

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=%QTDIR% ^
  -DFFMPEG_ROOT=%FFMPEG_ROOT% ^
  -DRTMIDI_SOURCE_DIR=%RTMIDI_SOURCE_DIR% ^
  "%SRC%"
if errorlevel 1 exit /b 1

cmake --build . --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b 1

copy /Y "%FFMPEG_ROOT%\bin\*.dll" "%BUILD%\" >nul
set "NDIDLL="
if defined NDI_RUNTIME_DIR_V6 set "NDIDLL=%NDI_RUNTIME_DIR_V6%\Processing.NDI.Lib.x64.dll"
if not defined NDIDLL set "NDIDLL=C:\Program Files (x86)\NDI\NDI 6 Runtime\v6\Processing.NDI.Lib.x64.dll"
if exist "%NDIDLL%" copy /Y "%NDIDLL%" "%BUILD%\" >nul
set "SPOUTDLL=%SRC%\third_party\Spout\bin\SpoutLibrary.dll"
if exist "%SPOUTDLL%" copy /Y "%SPOUTDLL%" "%BUILD%\" >nul
"%QTDIR%\bin\windeployqt.exe" --release --no-translations "%BUILD%\MultiPlayer.exe"
echo BUILD_OK
dir "%BUILD%\MultiPlayer.exe"
if exist "%BUILD%\Processing.NDI.Lib.x64.dll" echo NDI_DLL_OK
if exist "%BUILD%\SpoutLibrary.dll" echo SPOUT_DLL_OK
