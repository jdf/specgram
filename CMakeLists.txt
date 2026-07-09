cmake_minimum_required(VERSION 3.25)
project(specgram LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(KFR CONFIG REQUIRED)
find_package(SndFile CONFIG REQUIRED)
# The vcpkg cairo port ships pkg-config metadata only (no CMake config).
find_package(PkgConfig REQUIRED)
pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)

add_executable(specgram src/main.cpp)
target_link_libraries(specgram PRIVATE
    kfr
    kfr_dft
    SndFile::sndfile
    PkgConfig::cairo
)
