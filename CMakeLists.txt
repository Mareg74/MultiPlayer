cmake_minimum_required(VERSION 3.21)
project(MultiPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Identity: com.mareg74.multiplayer (macOS branch)
set(MP_APP_NAME "MultiPlayer")
set(MP_APP_ID "com.mareg74.multiplayer")
set(MP_MODULE_DEFAULT 2)
set(MP_MODULE_MAX 14)

option(MP_ENABLE_NDI "Build with NDI sender support" ON)

if(NOT APPLE)
    message(FATAL_ERROR "This CMakeLists belongs to the mac branch. Use the windows branch on Windows.")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# FFmpeg via pkg-config, with Homebrew fallback
find_package(PkgConfig)
set(FFMPEG_FOUND FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET
        libavformat libavcodec libavutil libswscale libswresample)
endif()

if(NOT FFMPEG_FOUND)
    if(DEFINED ENV{FFMPEG_ROOT})
        set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}")
    endif()
    find_path(FFMPEG_INCLUDE_DIRS libavcodec/avcodec.h
        PATHS ${FFMPEG_ROOT}/include /opt/homebrew/include /usr/local/include /usr/include)
    find_library(AVFORMAT_LIB avformat PATHS ${FFMPEG_ROOT}/lib /opt/homebrew/lib /usr/local/lib)
    find_library(AVCODEC_LIB avcodec PATHS ${FFMPEG_ROOT}/lib /opt/homebrew/lib /usr/local/lib)
    find_library(AVUTIL_LIB avutil PATHS ${FFMPEG_ROOT}/lib /opt/homebrew/lib /usr/local/lib)
    find_library(SWSCALE_LIB swscale PATHS ${FFMPEG_ROOT}/lib /opt/homebrew/lib /usr/local/lib)
    find_library(SWRESAMPLE_LIB swresample PATHS ${FFMPEG_ROOT}/lib /opt/homebrew/lib /usr/local/lib)
    if(FFMPEG_INCLUDE_DIRS AND AVFORMAT_LIB AND AVCODEC_LIB AND AVUTIL_LIB AND SWSCALE_LIB)
        set(FFMPEG_FOUND TRUE)
        set(FFMPEG_LIBRARIES ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB} ${SWRESAMPLE_LIB})
        message(STATUS "FFmpeg found via find_library: ${FFMPEG_INCLUDE_DIRS}")
    else()
        message(FATAL_ERROR "FFmpeg not found. Install ffmpeg (brew) or set FFMPEG_ROOT / PKG_CONFIG_PATH.")
    endif()
endif()

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

add_executable(MultiPlayer ${MP_SOURCES})

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
    MP_HAS_SPOUT=0
)

target_link_libraries(MultiPlayer PRIVATE
    Qt6::Widgets
    Qt6::Network
)

if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(MultiPlayer PRIVATE PkgConfig::FFMPEG)
else()
    target_include_directories(MultiPlayer PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(MultiPlayer PRIVATE ${FFMPEG_LIBRARIES})
endif()

# NDI: headers from third_party or NDI_SDK_DIR; runtime loaded dynamically
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
                "$ENV{HOME}/NDI SDK for Apple"
                "/Library/NDI SDK for Apple"
                "$ENV{HOME}/Library/NDI SDK for Apple")
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
        target_link_libraries(MultiPlayer PRIVATE ${CMAKE_DL_LIBS})
        message(STATUS "NDI enabled (dynamic load): ${_ndi_include}")
    else()
        target_compile_definitions(MultiPlayer PRIVATE MP_HAS_NDI=0)
        message(STATUS "NDI headers not found — building stub sender")
    endif()
else()
    target_compile_definitions(MultiPlayer PRIVATE MP_HAS_NDI=0)
endif()

# RtMidi (Homebrew)
if(DEFINED ENV{RTMIDI_SOURCE_DIR})
    set(RTMIDI_SOURCE_DIR "$ENV{RTMIDI_SOURCE_DIR}")
endif()
find_path(RTMIDI_INCLUDE_DIR RtMidi.h
    PATHS ${RTMIDI_SOURCE_DIR} /opt/homebrew/include/rtmidi /usr/local/include/rtmidi /usr/include/rtmidi)
find_library(RTMIDI_LIBRARY rtmidi PATHS /opt/homebrew/lib /usr/local/lib)
if(RTMIDI_LIBRARY AND RTMIDI_INCLUDE_DIR)
    target_include_directories(MultiPlayer PRIVATE ${RTMIDI_INCLUDE_DIR})
    target_link_libraries(MultiPlayer PRIVATE ${RTMIDI_LIBRARY})
    target_compile_definitions(MultiPlayer PRIVATE MP_HAS_RTMIDI=1)
    message(STATUS "RtMidi enabled: ${RTMIDI_LIBRARY}")
elseif(RTMIDI_INCLUDE_DIR AND EXISTS "${RTMIDI_INCLUDE_DIR}/RtMidi.cpp")
    target_sources(MultiPlayer PRIVATE "${RTMIDI_INCLUDE_DIR}/RtMidi.cpp")
    target_include_directories(MultiPlayer PRIVATE ${RTMIDI_INCLUDE_DIR})
    target_compile_definitions(MultiPlayer PRIVATE MP_HAS_RTMIDI=1 __MACOSX_CORE__)
    message(STATUS "RtMidi enabled (source): ${RTMIDI_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "RtMidi not found. brew install rtmidi")
endif()

find_library(COREMIDI_LIBRARY CoreMIDI)
find_library(COREAUDIO_LIBRARY CoreAudio)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
if(COREMIDI_LIBRARY)
    target_link_libraries(MultiPlayer PRIVATE
        ${COREMIDI_LIBRARY} ${COREAUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY})
endif()

set_target_properties(MultiPlayer PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "${MP_APP_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "${MP_APP_NAME}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

install(TARGETS MultiPlayer BUNDLE DESTINATION .)
