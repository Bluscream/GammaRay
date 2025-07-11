# This file is part of GammaRay, the Qt application inspection and manipulation tool.
#
# SPDX-FileCopyrightText: 2010 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#

# This is the top-level CMakeLists.txt file for the GammaRay project.
#
# Pass the following variables to cmake to control the build:
# (See INSTALL.md for more information)
#
# -DQT_VERSION_MAJOR=6
#  Build against QtX version
#  Default=(The first Qt version found by cmake or the one provided by CMAKE_PREFIX_PATH will be used)
#
# -DGAMMARAY_PROBE_ONLY_BUILD=[true|false]
#  Build only an additional probe configuration for an already existing launcher
#  Default=false
#
# -DGAMMARAY_CLIENT_ONLY_BUILD=[true|false]
#  Build the client part only
#  Default=false
#
# -DGAMMARAY_BUILD_UI=[true|false]
#  Build the GammaRay client and in-process UI
#  Default=true (false for QNX or Android)
#
# -DGAMMARAY_INSTALL_QT_LAYOUT=[true|false]
#  Install into Qt directory layout
#  Default=false
#
# -DGAMMARAY_MULTI_BUILD=[true|false]
#  Build multiple applicable probe configurations
#  Default=true
#
# -DGAMMARAY_BUILD_CLI_INJECTOR=[true|false]
#  Build command line injector on Windows
#  Default=true (ignored on non-Windows systems)
#
# -DGAMMARAY_STATIC_PROBE=[true|false]
#  Build the probe as static library for compile-time injection
#  Default=false
#
# -DGAMMARAY_CORE_ONLY_LAUNCHER=[true|false]
#  Only use QtCore in the CLI launcher (breaks style injector, but is needed for Boot2Qt compatibility
#  Default=false
#
# -DGAMMARAY_DISABLE_FEEDBACK=[true|false]
#  Disable user feedback support
#  Default=false (always true if cross-compiling or building probe-only (see GAMMARAY_PROBE_ONLY_BUILD))
#
# -DGAMMARAY_BUILD_DOCS=[true|false]
#  Build GammaRay documentation
#  Default=true (always false if cross-compiling or building probe-only (see GAMMARAY_PROBE_ONLY_BUILD))
#
# -DGAMMARAY_ENFORCE_QT_ASSERTS=[true|false]
#  Force QT_ASSERT in all builds
#  Default=false
#
# -DGAMMARAY_USE_PCH=[true|false]
#  Enable Precompiled Headers support
#  Default=false
#
# -DGAMMARAY_WITH_KDSME=[true|false]
#  Enable State Machine Viewer Plugin UI by requiring KDAB State Machine Editor
#  Default=false
#
# -DENABLE_GOLD_LINKER=[true|false]
#  Use GNU gold linker
#  Default=false
#
# -DWARNINGS_ARE_ERRORS=[true|false]
#  Enables -Werror
#  Default = false
#

cmake_minimum_required(VERSION 3.16.0)

file(STRINGS version.txt GAMMARAY_VERSION_FILE)
list(GET GAMMARAY_VERSION_FILE 0 GammaRay_VERSION)

project(
    GammaRay
    VERSION ${GammaRay_VERSION}
    LANGUAGES CXX C
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ECM/find-modules)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ECM/modules)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/KDAB/modules)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTOUIC TRUE)
set(CMAKE_AUTORCC TRUE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

if(NOT DEFINED CMAKE_SKIP_BUILD_RPATH)
    set(CMAKE_SKIP_BUILD_RPATH ON)
endif()
if(NOT DEFINED CMAKE_BUILD_WITH_INSTALL_RPATH)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
endif()
if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()
if(NOT DEFINED CMAKE_MACOSX_RPATH)
    set(CMAKE_MACOSX_RPATH TRUE)
endif()

# Set a default build type if none was specified
set(default_build_type "Release")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    set(default_build_type "Debug")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to ${default_build_type} as none was specified.")
    set(CMAKE_BUILD_TYPE
        "${default_build_type}"
        CACHE STRING "Choose the type of build." FORCE
    )
    # Set the possible values of build type for cmake-gui
    set_property(
        CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS
                 "Debug"
                 "Release"
                 "MinSizeRel"
                 "RelWithDebInfo"
    )
endif()

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckCXXSymbolExists)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CMakeParseArguments)
include(CMakePushCheckState)
include(CTest)
include(GammaRayMacros)
include(GammaRayMacrosInternal)
include(FeatureSummary)
set(KDE_INSTALL_USE_QT_SYS_PATHS OFF)
include(ECMGeneratePriFile)
include(ECMEnableSanitizers)
include(GenerateExportHeader)
include("3rdparty/backward-cpp/BackwardConfig.cmake")

# Exit for blacklisted compilers (those that don't support full C++11)
#  MSVC++ < 2015 aka 1900
#  Clang 3.1
set(BAD_CXX_MESSAGE "")
if(MSVC)
    if(MSVC_VERSION LESS 1900)
        set(BAD_CXX_MESSAGE "MSVC 2015 or higher")
    endif()
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.1.0)
        set(BAD_CXX_MESSAGE "Clang v3.1.0 or higher")
    endif()
endif()
if(BAD_CXX_MESSAGE)
    message(FATAL_ERROR "\nSorry, ${BAD_CXX_MESSAGE} is required to build this software. "
                        "Please retry using a compiler that supports C++11."
    )
endif()

set(GAMMARAY_REQUIRED_CXX_FEATURES
    cxx_lambdas
    cxx_nullptr
    cxx_range_for
    cxx_raw_string_literals
    cxx_uniform_initialization
    cxx_variadic_templates
    cxx_rvalue_references
    cxx_defaulted_functions
    cxx_deleted_functions
    cxx_override
    cxx_final
    cxx_nonstatic_member_init
)

# Version setup
set(GAMMARAY_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(GAMMARAY_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(GAMMARAY_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(GAMMARAY_VERSION ${PROJECT_VERSION})
set(GAMMARAY_VERSION_STRING "${GAMMARAY_VERSION}")
set(GAMMARAY_SOVERSION "${GAMMARAY_VERSION_MAJOR}.${GAMMARAY_VERSION_MINOR}.${GAMMARAY_VERSION_PATCH}")
set(GAMMARAY_PLUGIN_VERSION "${GAMMARAY_VERSION_MAJOR}.${GAMMARAY_VERSION_MINOR}")
set(PROJECT_VERSION_STRING "${GAMMARAY_VERSION_STRING}")
if(ANDROID)
    # non-rooted Android doesn't like .so versions and requires the lib prefix
    set(GAMMARAY_DEFAULT_LIBRARY_PROPERTIES PREFIX "lib")
else()
    set(GAMMARAY_DEFAULT_LIBRARY_PROPERTIES SOVERSION ${GAMMARAY_SOVERSION} VERSION ${GAMMARAY_SOVERSION})
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    find_package(Git)
    set_package_properties(
        Git PROPERTIES
        TYPE OPTIONAL
        PURPOSE "Determine exact build version."
    )
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _git_revision
        )
        string(REGEX REPLACE "\n" "" _git_revision "${_git_revision}")
        set(GAMMARAY_VERSION_STRING "${GAMMARAY_VERSION_STRING} (revision: ${_git_revision})")
    endif()
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config-gammaray-version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-gammaray-version.h
)

message(STATUS "Building GammaRay ${GAMMARAY_VERSION_STRING} in ${CMAKE_BUILD_TYPE} mode")

#
# Build options
#
set(GAMMARAY_BUILD_UI_DEFAULT ON)
if(QNXNTO OR ANDROID)
    set(GAMMARAY_BUILD_UI_DEFAULT OFF)
endif()

gammaray_option(
    GAMMARAY_PROBE_ONLY_BUILD "Build only an additional probe configuration for an already existing launcher." OFF
)

gammaray_option(GAMMARAY_CLIENT_ONLY_BUILD "Build the client part only." OFF)

if(GAMMARAY_CLIENT_ONLY_BUILD)
    set(GAMMARAY_BUILD_UI_DEFAULT ON)
endif()

if(GAMMARAY_PROBE_ONLY_BUILD)
    set(GAMMARAY_BUILD_UI_DEFAULT OFF)
endif()

gammaray_option(GAMMARAY_BUILD_UI "Build the GammaRay client and in-process UI." ${GAMMARAY_BUILD_UI_DEFAULT})

if(GAMMARAY_PROBE_ONLY_BUILD AND GAMMARAY_CLIENT_ONLY_BUILD)
    message(FATAL_ERROR "You can only use one of the *ONLY* option.")
endif()

gammaray_option(GAMMARAY_INSTALL_QT_LAYOUT "Install into Qt directory layout." OFF)
gammaray_option(GAMMARAY_MULTI_BUILD "Build multiple applicable probe configurations." ON)
gammaray_option(GAMMARAY_BUILD_CLI_INJECTOR "Build command line injector on Windows." ON)
gammaray_option(GAMMARAY_WITH_KDSME "Enable State Machine UI with KDSME" OFF)

set(GAMMARAY_BUILD_DOCS_DEFAULT ON)
set(GAMMARAY_DISABLE_FEEDBACK_DEFAULT OFF)
if(GAMMARAY_PROBE_ONLY_BUILD OR CMAKE_CROSSCOMPILING)
    set(GAMMARAY_BUILD_DOCS_DEFAULT OFF)
    set(GAMMARAY_DISABLE_FEEDBACK_DEFAULT ON)
endif()
gammaray_option(GAMMARAY_BUILD_DOCS "Build GammaRay documentation." ${GAMMARAY_BUILD_DOCS_DEFAULT})
gammaray_option(GAMMARAY_STATIC_PROBE "Build the probe as static library for compile-time injection." OFF)

gammaray_option(GAMMARAY_ENFORCE_QT_ASSERTS "Force QT_ASSERT in all builds." OFF)

gammaray_option(GAMMARAY_DISABLE_FEEDBACK "Disable user feedback support." ${GAMMARAY_DISABLE_FEEDBACK_DEFAULT})

gammaray_option(WARNINGS_ARE_ERRORS "Enables -Werror" OFF)

#
# Static probe setup
#
if(GAMMARAY_STATIC_PROBE)
    set(GAMMARAY_BUILD_UI OFF)
    set(GAMMARAY_PROBE_ONLY_BUILD ON)

    set(GAMMARAY_LIBRARY_TYPE STATIC)
    set(GAMMARAY_PLUGIN_TYPE STATIC)
else()
    set(GAMMARAY_LIBRARY_TYPE SHARED)
    set(GAMMARAY_PLUGIN_TYPE MODULE)
endif()

gammaray_option(GAMMARAY_USE_PCH "Enable Precompiled Headers support" OFF)
if(GAMMARAY_USE_PCH)
    add_library(gammaray_pch_core_gui STATIC)
    file(
        GENERATE
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/empty_pch.cpp
        CONTENT "/*dummy pch file*/"
    )
    target_sources(gammaray_pch_core_gui PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/empty_pch.cpp)
    target_link_libraries(gammaray_pch_core_gui PRIVATE Qt::Core Qt::Gui)
    set_target_properties(
        gammaray_pch_core_gui
        PROPERTIES PRECOMPILE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/core/pch.h
                   CXX_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN ON
                   POSITION_INDEPENDENT_CODE ON
    )
endif()

#
# Compiler & linker settings
#
function(append_if condition value)
    if(${condition})
        foreach(variable ${ARGN})
            set(${variable}
                "${${variable}} ${value}"
                PARENT_SCOPE
            )
        endforeach()
    endif()
endfunction()

# Add a C or C++ compiler flag if the compiler supports that flag
macro(add_flag_if_supported flag name)
    check_c_compiler_flag("-Werror ${flag}" "C_SUPPORTS_${name}")
    append_if("C_SUPPORTS_${name}" "${flag}" CMAKE_C_FLAGS)
    check_cxx_compiler_flag("-Werror ${flag}" "CXX_SUPPORTS_${name}")
    append_if("CXX_SUPPORTS_${name}" "${flag}" CMAKE_CXX_FLAGS)
endmacro()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_flag_if_supported(-Wunused-but-set-variable UNUSED_BUT_SET)
    add_flag_if_supported(-Wlogical-op LOGICAL_OP)
    add_flag_if_supported(-Wsizeof-pointer-memaccess POINTER_MEMACCESS)
    add_flag_if_supported(-Wreorder REORDER)
    add_flag_if_supported(-Wformat-security FORMAT_SECURITY)
    add_flag_if_supported(-Wsuggest-override SUGGEST_OVERRIDE)

    check_cxx_compiler_flag(-std=gnu++0x HAVE_GXX_GNUXX11)
    check_cxx_compiler_flag(-std=c++0x HAVE_GXX_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Woverloaded-virtual -Winit-self")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-include-dirs -Wunused -Wundef -Wpointer-arith")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-noreturn -Werror=return-type -Wswitch")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # -Wgnu-zero-variadic-macro-arguments (part of -pedantic) is triggered by every qCDebug()
        # call and therefore results in a lot of noise. This warning is only notifying us that clang
        # is emulating the GCC behaviour instead of the exact standard wording so we can safely ignore it
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments -Wno-c++20-extensions")
    endif()
    if(HAVE_GXX_GNUXX11) # QNX needs gnu++0x rather than c++0x for compiling QML V4 private headers
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x")
    elseif(HAVE_GXX_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    endif()
    if(MINGW)
        # mingw will error out on the crazy casts in probe.cpp without this
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
    endif()
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments -Wdocumentation")
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Qunused-arguments -Wdocumentation")
endif()

# Do not treat the operator name keywords and, bitand, bitor, compl, not, or and xor as synonyms as keywords.
# They're not supported under Visual Studio out of the box thus using them limits the portability of code
if(CMAKE_COMPILER_IS_GNUCXX
   OR CMAKE_C_COMPILER_ID MATCHES "Clang"
   OR (CMAKE_C_COMPILER_ID STREQUAL "Intel" AND NOT WIN32)
)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-operator-names")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-") #use strict C++ compliance
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244") #conversion from __int64 to int possible loss of data
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267") #conversion from size_t to int possible loss of data
endif()

if(QNXNTO)
    add_definitions(-D_QNX_SOURCE)
endif()

if(CYGWIN)
    add_definitions(-D_GNU_SOURCE)
endif()

if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_SYSTEM_NAME MATCHES Linux)
    # Gold linker
    gammaray_option(ENABLE_GOLD_LINKER "Use GNU gold linker" OFF)
    if(ENABLE_GOLD_LINKER)
        execute_process(
            COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version
            OUTPUT_VARIABLE stdout
            ERROR_QUIET
        )
        if("${stdout}" MATCHES "GNU gold")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fuse-ld=gold")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
        else()
            message(FATAL_ERROR "GNU gold linker isn't available.")
        endif()
    endif()
endif()

# linker flags
if((CMAKE_SYSTEM_NAME MATCHES Linux OR CMAKE_SYSTEM_NAME STREQUAL GNU) AND (CMAKE_COMPILER_IS_GNUCXX
                                                                            OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
)
    set(_linker_flags "-Wl,-z,origin")
    if(NOT ECM_ENABLE_SANITIZERS)
        set(_linker_flags "-Wl,--fatal-warnings -Wl,--no-undefined -lc ${_linker_flags}")
    endif()
    set(CMAKE_SHARED_LINKER_FLAGS "${_linker_flags} ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "${_linker_flags} ${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${_linker_flags} ${CMAKE_EXE_LINKER_FLAGS}")
endif()

if(GAMMARAY_ENFORCE_QT_ASSERTS)
    add_definitions(-DQT_FORCE_ASSERTS)
endif()

if(NOT DEFINED QT_VERSION_MAJOR)
    set(QT_VERSION_MAJOR 6)
endif()

set_package_properties(Qt${QT_VERSION_MAJOR}Core PROPERTIES TYPE REQUIRED)

find_package(Qt${QT_VERSION_MAJOR} NO_MODULE REQUIRED COMPONENTS Gui Network)
if(QT_VERSION_MAJOR EQUAL 6 AND QT_VERSION_MINOR LESS 3)
    message(FATAL_ERROR "Sorry, Qt6 less than version 6.3 is not supported since GammaRay 3.1")
endif()

find_package(
    Qt${QT_VERSION_MAJOR} NO_MODULE QUIET
    OPTIONAL_COMPONENTS
        3DAnimation
        3DExtras
        3DInput
        3DLogic
        3DRender
        3DQuick
        Bluetooth
        Concurrent
        Designer
        Location
        OpenGL
        Positioning
        Qml
        Quick
        QuickWidgets
        Svg
        Test
        WebEngineWidgets
        Widgets
        WaylandCompositor
)

if(ANDROID)
    find_package(Qt${QT_VERSION_MAJOR} NO_MODULE REQUIRED COMPONENTS AndroidExtras)
endif()

# Find these 'exotic' Qt modules without using find_package(... COMPONENTS)
# so we can retrieve those packages even when installed into a different prefix
find_package(
    Qt${QT_VERSION_MAJOR}Scxml
    ${Qt${QT_VERSION_MAJOR}_VERSION}
    EXACT
    NO_MODULE
    QUIET
)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(
        Qt${QT_VERSION_MAJOR}StateMachine
        ${Qt${QT_VERSION_MAJOR}_VERSION}
        EXACT
        NO_MODULE
        QUIET
    )
endif()

# if translation/doc host tools are missing, the Qt5 cmake config files throw errors...
if(GAMMARAY_BUILD_DOCS)
    find_package(Qt${QT_VERSION_MAJOR} NO_MODULE QUIET OPTIONAL_COMPONENTS Help LinguistTools)
endif()

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()
if(MINGW)
    # prevent 'file too big' and 'too many sections' errors with Qt6
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj")
endif()
if(Qt6Gui_VERSION VERSION_GREATER_EQUAL "6.10.0")
    find_package(Qt6WidgetsPrivate ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)
endif()

# these need to included after the last Qt-related find_package() call
include(ECMQt5ToQt6Porting)

if(APPLE)
    cmake_host_system_information(RESULT MACOS_RELEASE QUERY OS_RELEASE)
    message(STATUS "MACOS_RELEASE: ${MACOS_RELEASE}")
    message(STATUS "QtQuick_VERSION: ${QtQuick_VERSION}")
endif()

add_flag_if_supported(-Wzero-as-null-pointer-constant ZERO_AS_NULL_POINTER_CONSTANT)

if(GAMMARAY_BUILD_UI) # widgets are required for the UI
    find_package(Qt${QT_VERSION_MAJOR} NO_MODULE REQUIRED COMPONENTS Widgets)
endif()

set(HAVE_QT_CONCURRENT ${QtConcurrent_FOUND})
set(HAVE_QT_WIDGETS ${QtWidgets_FOUND})
set(HAVE_QT_SVG ${QtSvg_FOUND})
set(HAVE_QT_SCXML ${QtScxml_FOUND})
set(HAVE_QT_DESIGNER ${QtDesigner_FOUND})

if(TARGET Qt${QtGui_VERSION_MAJOR}::Gui)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_LIBRARIES Qt${QtGui_VERSION_MAJOR}::Gui)
    check_cxx_symbol_exists(QT_NO_OPENGL "QOpenGLContext" QT_NO_OPENGL)
    cmake_pop_check_state()
endif()

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x060500)
#add_definitions(-DQT_DEPRECATED_WARNINGS)

if(NOT DEFINED QT_LIBINFIX)
    set(QT_LIBINFIX "")
endif()

set_package_properties(Qt${QT_VERSION_MAJOR} PROPERTIES URL "https://qt.io/")
set_package_properties(
    Qt${QT_VERSION_MAJOR}Concurrent PROPERTIES
    TYPE RECOMMENDED
    PURPOSE "Required for the GammaRay launcher process list."
)
set_package_properties(
    Qt${QT_VERSION_MAJOR}Widget PROPERTIES
    TYPE RECOMMENDED
    PURPOSE "Required for the GammaRay client UI and widget-related tools."
)
set_package_properties(
    Qt${QT_VERSION_MAJOR}Svg PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for widget SVG export."
)

# debug suffixes for qmake compatibility
if(WIN32)
    set(CMAKE_DEBUG_POSTFIX "d")
elseif(APPLE)
    set(CMAKE_DEBUG_POSTFIX "_debug")
else()
    set(CMAKE_DEBUG_POSTFIX "")
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPERCASE_CMAKE_BUILD_TYPE)

set(GAMMARAY_PROBE_ABI_POSTFIX "")
if(NOT UPPERCASE_CMAKE_BUILD_TYPE MATCHES "^REL")
    set(GAMMARAY_PROBE_ABI_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
endif()

add_definitions(
    -DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS -DQT_NO_CAST_TO_ASCII -DQT_NO_URL_CAST_FROM_STRING
)
if(NOT WIN32)
    # BIC on MSVC at least (see e.g. https://bugreports.qt.io/browse/AUTOSUITE-946)
    add_definitions(-DQT_STRICT_ITERATORS)
endif()

if(UPPERCASE_CMAKE_BUILD_TYPE MATCHES "^RELEASE$")
    add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

if(HAVE_QT_WIDGETS)
    gammaray_option(
        GAMMARAY_CORE_ONLY_LAUNCHER
        "Only use QtCore in the CLI launcher (breaks style injector, but is needed for Boot2Qt compatibility)" FALSE
    )
else()
    set(GAMMARAY_CORE_ONLY_LAUNCHER TRUE)
endif()

add_feature_info("Widget .ui file export" HAVE_QT_DESIGNER "Requires QtDesigner library.")

#
# Additional dependencies
#

check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_symbol_exists(backtrace execinfo.h HAVE_BACKTRACE)
check_cxx_symbol_exists(abi::__cxa_demangle cxxabi.h HAVE_CXA_DEMANGLE)

# ELF header for ABI detection
find_file(HAVE_ELF_H elf.h)
find_file(HAVE_SYS_ELF_H sys/elf.h)
if(HAVE_ELF_H OR HAVE_SYS_ELF_H)
    set(HAVE_ELF TRUE)
endif()
add_feature_info("ELF ABI detection" HAVE_ELF "Automatic probe ABI detection on ELF-based systems. Requires elf.h.")

find_package(QmlLint)
set_package_properties(
    QmlLint PROPERTIES
    URL "https://qt.io"
    PURPOSE "Validate QML code."
)

find_package(KF${QtCore_VERSION_MAJOR}SyntaxHighlighting 6.0 NO_MODULE QUIET)
set_package_properties(
    KF${QtCore_VERSION_MAJOR}SyntaxHighlighting PROPERTIES
    TYPE RECOMMENDED
    URL "https://www.kde.org/"
    PURPOSE "Syntax highlighting for code editor."
)
if(TARGET KF${QtCore_VERSION_MAJOR}::SyntaxHighlighting)
    set(HAVE_SYNTAX_HIGHLIGHTING TRUE)
endif()

#
# Determine probe ABI
# this needs to be run after we know exactly what we are building, but is needed for that installation settings
include(GammaRayProbeABI)

#
# Installation settings
#
set(BUNDLE_INSTALL_DIR "")
if(ANDROID)
    set(GAMMARAY_INSTALL_QT_LAYOUT ON)
endif()
if(APPLE)
    set(BUNDLE_APP_NAME "GammaRay.app")

    if(GAMMARAY_INSTALL_QT_LAYOUT)
        set(BUNDLE_INSTALL_DIR "bin")
        set(RESOURCES_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/${BUNDLE_APP_NAME}/Contents/Resources")
        set(BIN_INSTALL_DIR "bin")
        set(INCLUDE_INSTALL_DIR "include/gammaray")
        set(LIB_INSTALL_DIR "lib")
        set(LIBEXEC_INSTALL_DIR "libexec")
        set(CMAKECONFIG_INSTALL_DIR "${LIB_INSTALL_DIR}/cmake/GammaRay")
        set(PLUGIN_INSTALL_DIR "${LIB_INSTALL_DIR}/gammaray")
        set(MAN_INSTALL_DIR "man/man1")
        set(DOC_INSTALL_DIR "doc/gammaray")
        set(QCH_INSTALL_DIR "doc")
        set(TRANSLATION_INSTALL_DIR "translations")
    else()
        # Make sure default prefix on mac is /Applications, dunnow why but it does not default to it
        # probably because we do not enabled built in bundle support in the main project
        string(COMPARE EQUAL "${CMAKE_INSTALL_PREFIX}" "/usr/local" CMP_RESULT)
        if(CMP_RESULT)
            set(CMAKE_INSTALL_PREFIX "/Applications")
        endif()

        set(BUNDLE_INSTALL_DIR ".")
        set(RESOURCES_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/${BUNDLE_APP_NAME}/Contents/Resources")
        set(BIN_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/${BUNDLE_APP_NAME}/Contents/MacOS")
        set(LIB_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/${BUNDLE_APP_NAME}/Contents/Frameworks")
        set(PLUGIN_INSTALL_DIR "${BUNDLE_INSTALL_DIR}/${BUNDLE_APP_NAME}/Contents/PlugIns/gammaray")
        set(LIBEXEC_INSTALL_DIR "${BIN_INSTALL_DIR}")
        set(MAN_INSTALL_DIR "${RESOURCES_INSTALL_DIR}/man/man1")
        set(DOC_INSTALL_DIR "${RESOURCES_INSTALL_DIR}/docs")
        set(QCH_INSTALL_DIR "${RESOURCES_INSTALL_DIR}/docs")
        set(TRANSLATION_INSTALL_DIR "${RESOURCES_INSTALL_DIR}/translations")
        set(INCLUDE_INSTALL_DIR "${RESOURCES_INSTALL_DIR}/include/gammaray")
        set(CMAKECONFIG_INSTALL_DIR "${RESOURCES_INSTALL_DIR}/cmake/GammaRay")
        set(ECM_MKSPECS_INSTALL_DIR "${RESOURCES_INSTALL_DIR}/${ECM_MKSPECS_INSTALL_DIR}")
    endif()
else()
    # Set installation paths
    # This takes care of installing into "lib64" on distros that use that, for instance,
    # by setting CMAKE_INSTALL_FULL_LIBDIR.
    include(GNUInstallDirs)

    set(BIN_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}") # relative, usually "bin"
    gammaray_convert_to_relative_path(BIN_INSTALL_DIR)
    if(GAMMARAY_INSTALL_QT_LAYOUT)
        set(LIB_INSTALL_DIR "lib") # Qt always uses "lib"
    else()
        set(LIB_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}") # "lib" or "lib64"
        gammaray_convert_to_relative_path(LIB_INSTALL_DIR)
    endif()
    set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/gammaray")
    set(CMAKECONFIG_INSTALL_DIR ${LIB_INSTALL_DIR}/cmake/GammaRay)
    set(DATAROOTDIR
        "${CMAKE_INSTALL_DATAROOTDIR}"
        CACHE PATH "Define install directory for read-only architecture-independent data"
    )
    gammaray_convert_to_relative_path(DATAROOTDIR)
    set(XDG_APPS_INSTALL_DIR "${DATAROOTDIR}/applications")
    set(APPDATA_INSTALL_DIR "${DATAROOTDIR}/metainfo")
    set(ICON_INSTALL_DIR "${DATAROOTDIR}/icons")
    set(MAN_INSTALL_DIR "${DATAROOTDIR}/man/man1")
    set(QCH_INSTALL_DIR
        "${CMAKE_INSTALL_DOCDIR}"
        CACHE PATH "Install location of Qt Assistant help files."
    )
    gammaray_convert_to_relative_path(QCH_INSTALL_DIR)
    set(ZSHAUTOCOMPLETE_INSTALL_DIR "${DATAROOTDIR}/zsh/site-functions")

    if(WIN32)
        set(PLUGIN_INSTALL_DIR "plugins/gammaray")
        set(LIBEXEC_INSTALL_DIR "${BIN_INSTALL_DIR}")
        set(DOC_INSTALL_DIR .)
        set(TRANSLATION_INSTALL_DIR "translations")
    else()
        set(PLUGIN_INSTALL_DIR "${LIB_INSTALL_DIR}/gammaray")
        set(LIBEXEC_INSTALL_DIR "${LIB_INSTALL_DIR}/gammaray/libexec")
        set(DOC_INSTALL_DIR "${DATAROOTDIR}/doc/gammaray/")
        set(TRANSLATION_INSTALL_DIR "${DATAROOTDIR}/gammaray/translations")
    endif()
endif()

if(NOT GAMMARAY_INSTALL_QT_LAYOUT)
    set(PROBE_BASENAME "gammaray_probe")
else()
    set(PROBE_BASENAME "${CMAKE_SHARED_LIBRARY_PREFIX}gammaray_probe")
endif()

if(GAMMARAY_INSTALL_QT_LAYOUT)
    if(WIN32)
        set(PROBE_INSTALL_DIR ${BIN_INSTALL_DIR})
    else()
        set(PROBE_INSTALL_DIR ${LIB_INSTALL_DIR})
    endif()
    set(PROBE_PLUGIN_INSTALL_DIR "plugins/gammaray")
    set(PLUGIN_INSTALL_DIR ${PROBE_PLUGIN_INSTALL_DIR})
    set(TARGET_PLUGIN_INSTALL_DIR "plugins/gammaray-target")
else()
    set(PROBE_INSTALL_DIR
        "${PLUGIN_INSTALL_DIR}/${GAMMARAY_PLUGIN_VERSION}/${GAMMARAY_PROBE_ABI}${GAMMARAY_PROBE_ABI_POSTFIX}"
    )
    set(PROBE_PLUGIN_INSTALL_DIR ${PROBE_INSTALL_DIR})
    set(TARGET_PLUGIN_INSTALL_DIR "${PROBE_INSTALL_DIR}/target")
endif()

if(NOT GAMMARAY_OUTPUT_PREFIX) # set by multibuild
    set(GAMMARAY_OUTPUT_PREFIX ${PROJECT_BINARY_DIR})
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${GAMMARAY_OUTPUT_PREFIX}/${BIN_INSTALL_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${GAMMARAY_OUTPUT_PREFIX}/${LIB_INSTALL_DIR})

set(INSTALL_TARGETS_DEFAULT_ARGS
    RUNTIME
    DESTINATION
    ${BIN_INSTALL_DIR}
    LIBRARY
    DESTINATION
    ${LIB_INSTALL_DIR}
    ARCHIVE
    DESTINATION
    ${LIB_INSTALL_DIR}
    COMPONENT
    Devel
    BUNDLE
    DESTINATION
    ${BUNDLE_INSTALL_DIR}
)

# "inverse" install dirs, to find the base location again
if(APPLE)
    set(BUNDLE_INSTALL_DIR_PRIVATE "${BUNDLE_INSTALL_DIR}/${BUNDLE_APP_NAME}/Contents/MacOS")
    gammaray_inverse_dir(GAMMARAY_INVERSE_BUNDLE_DIR "${BUNDLE_INSTALL_DIR_PRIVATE}")
else()
    set(BUNDLE_INSTALL_DIR_PRIVATE "")
    set(GAMMARAY_INVERSE_BUNDLE_DIR "")
endif()
gammaray_inverse_dir(GAMMARAY_INVERSE_BIN_DIR "${BIN_INSTALL_DIR}")
gammaray_inverse_dir(GAMMARAY_INVERSE_LIB_DIR "${LIB_INSTALL_DIR}")
gammaray_inverse_dir(GAMMARAY_INVERSE_PROBE_DIR "${PROBE_INSTALL_DIR}")
gammaray_inverse_dir(GAMMARAY_INVERSE_LIBEXEC_DIR "${LIBEXEC_INSTALL_DIR}")

#
# actually build the stuff
#
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-gammaray.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-gammaray.h)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty ${CMAKE_BINARY_DIR})

include(KDQtInstallPaths) #to set QT_INSTALL_FOO variables
add_subdirectory(cmake)
add_subdirectory(3rdparty)

# Enable WARNINGS_ARE_ERRORS AFTER 3rdparty modules
if(WARNINGS_ARE_ERRORS)
    if(NOT MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-error=deprecated-declarations")

        if(CMAKE_COMPILER_IS_GNUCXX)
            # each compiler has its own opinion on when to apply [noreturn]
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=suggest-attribute=noreturn")

            if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0)
                # QTBUG-135486
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=array-bounds")
            endif()
        endif()
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
        add_definitions(-D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    endif()
endif()

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE -D_USING_V110_SDK71_=1)
endif()

add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(probe)
add_subdirectory(launcher)
if(GAMMARAY_BUILD_UI AND TARGET Qt::Widgets)
    if(NOT GAMMARAY_DISABLE_FEEDBACK)
        add_subdirectory(3rdparty/kuserfeedback)
    endif()
    add_subdirectory(ui)
    if(NOT GAMMARAY_CLIENT_ONLY_BUILD)
        add_subdirectory(inprocessui)
    endif()
    if(NOT GAMMARAY_PROBE_ONLY_BUILD)
        add_subdirectory(client)
        add_subdirectory(app)
    endif()
endif()
if(BUILD_TESTING
   AND TARGET Qt::Test
   AND NOT CMAKE_CROSSCOMPILING
)
    add_subdirectory(tests)
endif()
add_subdirectory(plugins)
if(QtLinguistTools_FOUND)
    add_subdirectory(translations)
endif()
if(GAMMARAY_MULTI_BUILD)
    add_subdirectory(multibuild)
endif()
if(GAMMARAY_BUILD_DOCS)
    add_subdirectory(examples)
    add_subdirectory(docs) # needs to go last, so see all installed headers for the API docs
endif()

# IDE Support (e.g. for QtCreator): Make the IDE aware of other files as well
file(GLOB ALL_OTHER_FILES_LIST "*.md")
add_custom_target(
    ALL_OTHER_FILES
    SOURCES ${ALL_OTHER_FILES_LIST}
    COMMENT "Target for IDE support to make the IDE aware of other files"
)

set(LICENSE_FILE "LICENSE.txt")
set(README_FILE "README.md")
list(APPEND DOCS ${LICENSE_FILE} ${README_FILE})
if(NOT APPLE AND NOT GAMMARAY_PROBE_ONLY_BUILD)
    if(UNIX AND GAMMARAY_BUILD_UI)
        install(FILES GammaRay.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
        install(FILES com.kdab.GammaRay.metainfo.xml DESTINATION ${APPDATA_INSTALL_DIR})
    endif()
    install(FILES ${DOCS} DESTINATION ${DOC_INSTALL_DIR})
    install(DIRECTORY LICENSES DESTINATION ${DOC_INSTALL_DIR})
endif()

#
# cppcheck
#
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
    set(_cppcheck_flags "-I${CMAKE_CURRENT_BINARY_DIR}")
    get_directory_property(_inc_dirs INCLUDE_DIRECTORIES)
    foreach(_current ${_inc_dirs})
        set(_cppcheck_flags ${_cppcheck_flags} "-I${_current}")
    endforeach()
    get_directory_property(_defs COMPILE_DEFINITIONS)
    foreach(_current ${_defs})
        set(_cppcheck_flags ${_cppcheck_flags} "-D${_current}")
    endforeach()

    # cmake-lint: disable=C0301
    add_custom_target(
        cppcheck
        COMMAND
            ${CPPCHECK_EXECUTABLE} --enable=all --inconclusive -f --suppress=*:${QT_INSTALL_HEADERS}* ${_cppcheck_flags}
            -i${CMAKE_CURRENT_SOURCE_DIR}/3rdparty -i${CMAKE_CURRENT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running the cppcheck static code checker"
    )
endif()

#
# CMake package config file generation
#
if(NOT GAMMARAY_PROBE_ONLY_BUILD)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/GammaRayConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/GammaRayConfig.cmake
        INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
        PATH_VARS INCLUDE_INSTALL_DIR
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/GammaRayConfigVersion.cmake
        VERSION ${GAMMARAY_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/GammaRayConfig.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/GammaRayConfigVersion.cmake DESTINATION ${CMAKECONFIG_INSTALL_DIR}
    )

    install(
        EXPORT GammaRayTargets
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        FILE GammaRayTarget.cmake
        #     NAMESPACE GammaRay::
    )
endif()

#CPACK: General Settings
set(CPACK_GENERATOR "TBZ2")
set(CPACK_PACKAGE_NAME "gammaray")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "An introspection tool for Qt applications")
#TODO: shorten lines in the Readme.txt to make rpmlint happy
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/${README_FILE}")
set(CPACK_PACKAGE_VENDOR "Klaralvdalens Datakonsult AB (KDAB)")
set(CPACK_PACKAGE_CONTACT "gammaray-devel@kdab.com")
set(CPACK_PACKAGE_VERSION_MAJOR "${GAMMARAY_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${GAMMARAY_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${GAMMARAY_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION "${GAMMARAY_VERSION}")

#CPACK: RPM Specific Settings
set(CPACK_RPM_PACKAGE_LICENSE "GPLv2+")
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")

#CPACK: DEB Specific Settings
set(CPACK_DEBIAN_PACKAGE_SECTION "Development")

set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/${README_FILE}")
set(ICONS_DIR "${CMAKE_SOURCE_DIR}/ui/resources/gammaray")
if(WIN32)
    set(CPACK_GENERATOR "NSIS" "ZIP")
    set(CPACK_PACKAGE_EXECUTABLES "GammaRay" "GammaRay")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "GammaRay")
    set(CPACK_PACKAGE_FILE_NAME "GammaRay ${GAMMARAY_VERSION}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/${LICENSE_FILE}")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${BIN_INSTALL_DIR}")
    set(CPACK_NSIS_MANIFEST_DPI_AWARE TRUE)
    set(CPACK_NSIS_MUI_ICON "${ICONS_DIR}/GammaRay.ico")
    #set(CPACK_PACKAGE_ICON "${ICONS_DIR}/TODO.bmp")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://www.kdab.com/")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "GammaRay${CMAKE_EXECUTABLE_SUFFIX}")
    set(CPACK_NSIS_MENU_LINKS "${LICENSE_FILE}" "License" "${README_FILE}" "Readme")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${CPACK_NSIS_INSTALLED_ICON_NAME}")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_VOLUME_NAME "GammaRay")
    set(CPACK_SYSTEM_NAME "OSX")
    set(CPACK_PACKAGE_FILE_NAME "GammaRay-${GAMMARAY_VERSION}")
    set(CPACK_PACKAGE_ICON "${ICONS_DIR}/GammaRay.icns")
    # set(CPACK_DMG_DS_STORE "${ICONS_DIR}/TODO_DSStore")
    # set(CPACK_DMG_BACKGROUND_IMAGE "${ICONS_DIR}/TODO_DMGBackground.png")
elseif(UNIX)
    set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()

include(CPack)
include(PackageIFW)

addpackagetarget(GammaRay gammaray_probe install lib)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
