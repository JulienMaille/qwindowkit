cmake_minimum_required(VERSION 3.19)

project(QWindowKit
    VERSION 1.4.1.0
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/stdware/qwindowkit"
    DESCRIPTION "Cross-platform window customization framework"
)

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(LINUX AND NOT CMAKE_CROSSCOMPILING)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

set(QWINDOWKIT_IS_TOP_LEVEL ON)
if("x${CMAKE_SOURCE_DIR}" STREQUAL "x${PROJECT_SOURCE_DIR}")
    message("QWindowKit: Building as a top-level project.")
else()
    set(QWINDOWKIT_IS_TOP_LEVEL OFF)
    message("QWindowKit: Building as a subproject.")
endif()

# ----------------------------------
# Build Options
# ----------------------------------
option(QWINDOWKIT_BUILD_STATIC "Build static libraries" OFF)
option(QWINDOWKIT_BUILD_WIDGETS "Build widgets module" ON)
option(QWINDOWKIT_BUILD_QUICK "Build quick module" OFF)
option(QWINDOWKIT_BUILD_EXAMPLES "Build examples" OFF)
# option(QWINDOWKIT_BUILD_DOCUMENTATIONS "Build documentations" OFF) # Documentation build removed
option(QWINDOWKIT_INSTALL "Install library" ON)

option(QWINDOWKIT_ENABLE_QT_WINDOW_CONTEXT "Enable Qt Window Context anyway" OFF)
option(QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS "Enable system borders on Windows" ON)
option(QWINDOWKIT_ENABLE_STYLE_AGENT "Enable building style agent" ON)

#[[

Detailed Introcuction to Configure Options:

`QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS`
  - If you don't want the system borders on Windows 10/11, you can DISABLE this option.
  - If so, the Windows 10 top border issue will disappear. However, part of the client edge
    area will be occupied as the resizing margins.

`QWINDOWKIT_ENABLE_QT_WINDOW_CONTEXT`
  - If you want to use pure Qt emulated frameless implementation, you can ENABLE this option.
  - If so, all system native features will be lost.

`QWINDOWKIT_ENABLE_STYLE_AGENT`
  - Select whether to exclude the style component by DISABLING this option according to your
    requirements and your Qt version.

#]]

# ----------------------------------
# CMake Settings
# ----------------------------------
if(QWINDOWKIT_IS_TOP_LEVEL)
    if(MSVC)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

        if(NOT DEFINED CMAKE_DEBUG_POSTFIX)
            set(CMAKE_DEBUG_POSTFIX "d")
        endif()
    elseif(MINGW)
        set(CMAKE_STATIC_LIBRARY_PREFIX "")
        set(CMAKE_SHARED_LIBRARY_PREFIX "")
    endif()
endif()

if(QWINDOWKIT_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif()

# ----------------------------------
# Project Variables
# ----------------------------------
set(QWINDOWKIT_VERSION ${PROJECT_VERSION})
set(QWINDOWKIT_INSTALL_NAME ${PROJECT_NAME})

string(TIMESTAMP _QWINDOWKIT_CURRENT_YEAR "%Y")
set(QWINDOWKIT_COPYRIGHT "Copyright 2023-${_QWINDOWKIT_CURRENT_YEAR} Stdware Collections")
set(QWINDOWKIT_DESCRIPTION ${PROJECT_DESCRIPTION})

# ----------------------------------
# Find basic dependencies
# ----------------------------------
# qmsetup dependency removed

# Set up output directories (replacement for qm_init_directories)
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

# ----------------------------------
# Add source modules
# ----------------------------------
add_subdirectory(src)

if(QWINDOWKIT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
