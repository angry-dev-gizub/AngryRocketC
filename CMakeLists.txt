cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#--------------------------------------------------------------------
# Version
#--------------------------------------------------------------------

file(
    STRINGS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    ARC_VERSION
    LIMIT_COUNT 1
)

if (NOT ARC_VERSION)
    message(FATAL_ERROR "Failed to read Arc version")
endif()

if (NOT ARC_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(
        FATAL_ERROR
        "Invalid Arc version '${ARC_VERSION}'. Expected MAJOR.MINOR.PATCH."
    )
endif()

#--------------------------------------------------------------------
# Project
#--------------------------------------------------------------------

project(AngryRocketC VERSION ${ARC_VERSION} LANGUAGES C CXX)

#--------------------------------------------------------------------
# Global settings
#--------------------------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(ARC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ARC_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

string(
    COMPARE EQUAL
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"
    ARC_STANDALONE
)

#--------------------------------------------------------------------
# Output directories
#--------------------------------------------------------------------

set(
    CMAKE_RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/bin"
)

#--------------------------------------------------------------------
# Options
#--------------------------------------------------------------------

option(
    ARC_BUILD_LSP
    "Build the Arc Language Server"
    OFF # for now.
)

option(
    ARC_BUILD_TESTS
    "Build Arc tests"
    ${ARC_STANDALONE}
)

option(
    ARC_ENABLE_CLANG_TIDY
    "Run clang-tidy while compiling Arc targets"
    OFF
)

#--------------------------------------------------------------------
# Platform
#--------------------------------------------------------------------

if (WIN32)
    set(ARC_PLATFORM_WINDOWS ON)

elseif (UNIX AND NOT APPLE)
    set(ARC_PLATFORM_LINUX ON)
elseif (APPLE)
    set(ARC_PLATFORM_APPLE)

else()
    message(FATAL_ERROR "Unsupported platform")
endif()

#--------------------------------------------------------------------
# Subdirectories
#--------------------------------------------------------------------

add_subdirectory(external)
add_subdirectory(cmake_config)
add_subdirectory(ARC-CORE)
add_subdirectory(ARC)

if (ARC_BUILD_LSP)
    add_subdirectory(ARC-LSP)
endif()

if (ARC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (ARC_STANDALONE)
    add_subdirectory(docs)
endif()