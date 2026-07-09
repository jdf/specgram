cmake_minimum_required(VERSION 3.25)

# Locate the vcpkg toolchain: an explicit CMAKE_TOOLCHAIN_FILE wins, then
# $VCPKG_ROOT, then ~/vcpkg.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_root "$ENV{VCPKG_ROOT}")
    else()
        set(_vcpkg_root "$ENV{HOME}/vcpkg")
    endif()
    set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
endif()
if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(FATAL_ERROR
        "vcpkg toolchain not found at ${CMAKE_TOOLCHAIN_FILE}; "
        "set VCPKG_ROOT (or CMAKE_TOOLCHAIN_FILE) to your vcpkg checkout")
endif()

project(specgram LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(KFR CONFIG REQUIRED)
find_package(SndFile CONFIG REQUIRED)
# The vcpkg cairo port ships pkg-config metadata only (no CMake config).
find_package(PkgConfig REQUIRED)
pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)

add_executable(specgram src/main.cc)
target_link_libraries(specgram PRIVATE
    kfr
    kfr_dft
    SndFile::sndfile
    PkgConfig::cairo
)
