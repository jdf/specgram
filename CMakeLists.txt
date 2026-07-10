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

project(specgram VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(KFR CONFIG REQUIRED)
find_package(SndFile CONFIG REQUIRED)
# The vcpkg cairo port ships pkg-config metadata only (no CMake config).
find_package(PkgConfig REQUIRED)
pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)

# The rendering pipeline as a library. The public headers (dsp.h, render.h)
# depend only on the standard library; kfr, libsndfile, and cairo stay
# implementation details of the .cc files.
add_library(specgram_lib STATIC
    src/dsp.cc
    src/render.cc
)
add_library(specgram::specgram ALIAS specgram_lib)
set_target_properties(specgram_lib PROPERTIES
    OUTPUT_NAME specgram
    EXPORT_NAME specgram
)
target_include_directories(specgram_lib PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(specgram_lib PRIVATE
    kfr
    kfr_dft
    SndFile::sndfile
    PkgConfig::cairo
)

# The CLI is optional so packaging (e.g. a vcpkg port) can build just the
# library.
option(SPECGRAM_BUILD_CLI "Build the specgram command-line tool" ON)
if(SPECGRAM_BUILD_CLI)
    add_executable(specgram src/main.cc)
    target_link_libraries(specgram PRIVATE specgram_lib)
endif()

# Install rules: library, CLI, and a relocatable CMake package so consumers
# (including a vcpkg port) can find_package(specgram) and link
# specgram::specgram. Headers install under include/specgram/; render.h's
# relative include of dsp.h resolves within that directory.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS specgram_lib EXPORT specgram-targets)
if(SPECGRAM_BUILD_CLI)
    install(TARGETS specgram)
endif()
install(FILES src/dsp.h src/render.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/specgram")
install(EXPORT specgram-targets
    NAMESPACE specgram::
    DESTINATION "${CMAKE_INSTALL_DATADIR}/specgram")

configure_package_config_file(
    cmake/specgram-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/specgram-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/specgram")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/specgram-config-version.cmake"
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/specgram-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/specgram-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/specgram")
