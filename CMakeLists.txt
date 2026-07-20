cmake_minimum_required(VERSION 3.21)
project(MultiPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Identity: com.mareg74.multiplayer (Windows branch)
set(MP_APP_NAME "MultiPlayer")
set(MP_APP_ID "com.mareg74.multiplayer")
set(MP_MODULE_DEFAULT 2)
set(MP_MODULE_MAX 14)

option(MP_ENABLE_NDI "Build with NDI sender support" ON)
option(MP_ENABLE_SPOUT "Build with Spout sender" ON)

if(NOT WIN32)
    message(FATAL_ERROR "This CMakeLists belongs to the windows branch. Use the mac branch on macOS.")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# FFmpeg: FFMPEG_ROOT or C:/deps/ffmpeg (see scripts/win-fetch-deps.ps1)
if(DEFINED ENV{FFMPEG_ROOT})
    set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}")
endif()
if(NOT FFMPEG_ROOT)
    set(FFMPEG_ROOT "C:/deps/ffmpeg")
endif()

find_path(FFMPEG_INCLUDE_DIRS libavcodec/avcodec.h
    PATHS ${FFMPEG_ROOT}/include)
find_library(AVFORMAT_LIB avformat PATHS ${FFMPEG_ROOT}/lib)
find_library(AVCODEC_LIB avcodec PATHS ${FFMPEG_ROOT}/lib)
find_library(AVUTIL_LIB avutil PATHS ${FFMPEG_ROOT}/lib)
find_library(SWSCALE_LIB swscale PATHS ${FFMPEG_ROOT}/lib)
find_library(SWRESAMPLE_LIB swresample PATHS ${FFMPEG_ROOT}/lib)
if(NOT (FFMPEG_INCLUDE_DIRS AND AVFORMAT_LIB AND AVCODEC_LIB AND AVUTIL_LIB AND SWSCALE_LIB))
    message(FATAL_ERROR "FFmpeg not found. Run scripts/win-fetch-deps.ps1 or set FFMPEG_ROOT.")
endif()
set(FFMPEG_LIBRARIES ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB} ${SWRESAMPLE_LIB})
message(STATUS "FFmpeg: ${FFMPEG_INCLUDE_DIRS}")

set(MP_SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/PlayerModuleWidget.cpp
    src/ui/VideoPreviewWidget.cpp
    src/ui/OutputButton.cpp
    src/ui/StatsBar.cpp
    src/ui/ClipTransformPanel.cpp
    src/ui/ProgramCanvasWidget.cpp
    src/ui/OutputMapDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/SliceWarpEditor.cpp
    src/media/Decoder.cpp
    src/media/PlayerModule.cpp
    src/media/MediaBank.cpp
    src/compose/OutputCanvas.cpp
    src/output/OutputManager.cpp
    src/output/NdiSender.cpp
    src/output/SpoutSender.cpp
    src/map/OutputMap.cpp
    src/map/ResolumeXmlImport.cpp
    src/project/ProjectFile.cpp
    src/core/PerfMonitor.cpp
    src/control/OscServer.cpp
    src/control/MidiController.cpp
    src/control/ControlRouter.cpp
)

add_executable(MultiPlayer WIN32 ${MP_SOURCES})

target_include_directories(MultiPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

target_compile_definitions(MultiPlayer PRIVATE
    MP_APP_NAME="${MP_APP_NAME}"
    MP_APP_ID="${MP_APP_ID}"
    MP_MODULE_DEFAULT=${MP_MODULE_DEFAULT}
    MP_MODULE_MAX=${MP_MODULE_MAX}
    MP_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(MultiPlayer PRIVATE
    Qt6::Widgets
    Qt6::Network
    ${FFMPEG_LIBRARIES}
    psapi
)

# NDI
if(MP_ENABLE_NDI)
    if(DEFINED ENV{NDI_SDK_DIR})
        set(NDI_SDK_DIR "$ENV{NDI_SDK_DIR}")
    endif()
    set(_ndi_include "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NDI/include/Processing.NDI.Lib.h")
        set(_ndi_include "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NDI/include")
    endif()
    if(NOT _ndi_include AND NOT NDI_SDK_DIR)
        foreach(_g
                "C:/Program Files/NDI/NDI 6 SDK"
                "C:/Program Files/NDI/NDI 5 SDK"
                "C:/Program Files/NDI/NDI SDK")
            if(EXISTS "${_g}")
                set(NDI_SDK_DIR "${_g}")
                break()
            endif()
        endforeach()
    endif()
    if(NOT _ndi_include AND NDI_SDK_DIR)
        if(EXISTS "${NDI_SDK_DIR}/Include/Processing.NDI.Lib.h")
            set(_ndi_include "${NDI_SDK_DIR}/Include")
        elseif(EXISTS "${NDI_SDK_DIR}/include/Processing.NDI.Lib.h")
            set(_ndi_include "${NDI_SDK_DIR}/include")
        endif()
    endif()
    if(_ndi_include)
        target_include_directories(MultiPlayer PRIVATE "${_ndi_include}")
        target_compile_definitions(MultiPlayer PRIVATE MP_HAS_NDI=1)
        message(STATUS "NDI enabled (dynamic load): ${_ndi_include}")
    else()
        target_compile_definitions(MultiPlayer PRIVATE MP_HAS_NDI=0)
        message(STATUS "NDI headers not found — building stub sender")
    endif()
else()
    target_compile_definitions(MultiPlayer PRIVATE MP_HAS_NDI=0)
endif()

# Spout
if(MP_ENABLE_SPOUT)
    if(DEFINED ENV{SPOUT_SDK_DIR})
        set(SPOUT_SDK_DIR "$ENV{SPOUT_SDK_DIR}")
    endif()
    set(_spout_include "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Spout/include/SpoutLibrary.h")
        set(_spout_include "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Spout/include")
    elseif(SPOUT_SDK_DIR AND EXISTS "${SPOUT_SDK_DIR}/include/SpoutLibrary.h")
        set(_spout_include "${SPOUT_SDK_DIR}/include")
    endif()
    if(_spout_include)
        target_include_directories(MultiPlayer PRIVATE "${_spout_include}")
        target_compile_definitions(MultiPlayer PRIVATE MP_HAS_SPOUT=1)
        message(STATUS "Spout enabled (dynamic load): ${_spout_include}")
    else()
        target_compile_definitions(MultiPlayer PRIVATE MP_HAS_SPOUT=0)
        message(STATUS "Spout SDK not found — stub sender (set SPOUT_SDK_DIR)")
    endif()
else()
    target_compile_definitions(MultiPlayer PRIVATE MP_HAS_SPOUT=0)
endif()

# RtMidi
if(DEFINED ENV{RTMIDI_SOURCE_DIR})
    set(RTMIDI_SOURCE_DIR "$ENV{RTMIDI_SOURCE_DIR}")
endif()
if(NOT RTMIDI_SOURCE_DIR)
    set(RTMIDI_SOURCE_DIR "C:/deps/rtmidi")
endif()
find_path(RTMIDI_INCLUDE_DIR RtMidi.h PATHS ${RTMIDI_SOURCE_DIR} C:/deps/rtmidi)
find_library(RTMIDI_LIBRARY rtmidi PATHS C:/deps/rtmidi/build)
if(RTMIDI_LIBRARY AND RTMIDI_INCLUDE_DIR)
    target_include_directories(MultiPlayer PRIVATE ${RTMIDI_INCLUDE_DIR})
    target_link_libraries(MultiPlayer PRIVATE ${RTMIDI_LIBRARY})
    target_compile_definitions(MultiPlayer PRIVATE MP_HAS_RTMIDI=1)
    message(STATUS "RtMidi enabled: ${RTMIDI_LIBRARY}")
elseif(RTMIDI_INCLUDE_DIR AND EXISTS "${RTMIDI_INCLUDE_DIR}/RtMidi.cpp")
    target_sources(MultiPlayer PRIVATE "${RTMIDI_INCLUDE_DIR}/RtMidi.cpp")
    target_include_directories(MultiPlayer PRIVATE ${RTMIDI_INCLUDE_DIR})
    target_compile_definitions(MultiPlayer PRIVATE MP_HAS_RTMIDI=1 __WINDOWS_MM__)
    target_link_libraries(MultiPlayer PRIVATE winmm)
    message(STATUS "RtMidi enabled (source): ${RTMIDI_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "RtMidi not found. Run scripts/win-fetch-deps.ps1")
endif()

set_target_properties(MultiPlayer PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME MultiPlayer
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/app.rc")
    target_sources(MultiPlayer PRIVATE resources/windows/app.rc)
endif()

install(TARGETS MultiPlayer RUNTIME DESTINATION bin)
