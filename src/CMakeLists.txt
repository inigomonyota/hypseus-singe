cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0079 NEW)

set(PKG_VERSION "v2.11.7")

project(hypseus)

option(BENCHMARK        "Benchmark"             OFF)
option(DEBUG            "Debug"                 OFF)
option(VLDP_DEBUG       "VLDP Debug"            OFF)
option(CPU_DEBUG        "CPU Debug"             OFF)
option(BUILD_SINGE      "Singe"                 ON)
option(BUILDBOT         "Buildbot"              OFF)
option(ASAN             "Address Sanitizer"     OFF)

# Abstract out -retropath ES extension: Use .hypseus NOT .daphne
option(ABSTRACT_SINGE   "Redefine Singe LUA Rewrites"     ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if (UNIX)
  if (CMAKE_SIZEOF_VOID_P EQUAL 4)
      message(STATUS "Detected 32bit: Compiling with -D_FILE_OFFSET_BITS=64")
      add_definitions(-D_FILE_OFFSET_BITS=64)
  endif()
endif()

set(DOWNLOAD_ARGS)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    list(APPEND DOWNLOAD_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

# Set compiler warning flags
if (MSVC)
    add_compile_options(/W4)
    # Disable specific MSVC warnings that are too noisy
    add_compile_options(/wd4244 /wd4267 /wd4996)
else()
    add_compile_options(-Wall)
endif()

if (ASAN)
    if (NOT MSVC)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(TargetArch)
target_architecture(CMAKE_TARGET_ARCHITECTURES)

# Config.h defines
string(TOUPPER ${CMAKE_SYSTEM_NAME} TARGET_SYSTEM)
string(CONCAT TARGET_SYSTEM TARGET_ ${TARGET_SYSTEM})
string(TOUPPER ${CMAKE_TARGET_ARCHITECTURES} TARGET_ARCH)
string(CONCAT TARGET_ARCH TARGET_ ${TARGET_ARCH})

include(BuildSupport)
include(GNUInstallDirs)
include(GetGitRevisionDescription)
include(InstallRequiredSystemLibraries)
include(ExternalProject)

# Only used on non-MSVC toolchains
if(NOT WIN32 OR NOT MSVC)
    include(FindPkgConfig)
endif()

if(WIN32 AND MSVC)
    # vcpkg libmpeg2 does not reliably export a *Config.cmake*, so do manual lookup.
    find_path(LIBMPEG2_INCLUDE_DIR NAMES mpeg2dec/mpeg2.h)
    find_library(LIBMPEG2_LIBRARY NAMES mpeg2 mpeg2dec libmpeg2)

    if(NOT LIBMPEG2_INCLUDE_DIR OR NOT LIBMPEG2_LIBRARY)
        message(FATAL_ERROR "libmpeg2 not found. Run: vcpkg install libmpeg2:x64-windows")
    endif()

    add_library(hypseus_libmpeg2 INTERFACE)
    target_include_directories(hypseus_libmpeg2 INTERFACE ${LIBMPEG2_INCLUDE_DIR})
    target_link_libraries(hypseus_libmpeg2 INTERFACE ${LIBMPEG2_LIBRARY})
else()
    include(BuildLibMPEG2)
    build_libmpeg2()
endif()

use_cxx11()

message(STATUS "Target: ${CMAKE_SYSTEM_NAME} ${CMAKE_TARGET_ARCHITECTURES}")

git_describe(VER_TAG --tags --abbrev=0)
if("${VER_TAG}" MATCHES "-NOTFOUND")
    set(VER_TAG  ${PKG_VERSION})
    set(VER_GIT_COUNT "0")
else()
    git_revlist(VER_GIT_COUNT --count ${VER_TAG}..HEAD)
endif()
string(REGEX REPLACE "^v" "" RESULT ${VER_TAG})
string(REGEX REPLACE "[-\\.]" ";" RESULT ${RESULT})
list(GET RESULT 0 VER_MAJOR)
list(GET RESULT 1 VER_MINOR)
list(GET RESULT 2 VER_PATCH)
if(NOT "${VER_GIT_COUNT}" EQUAL "0")
    set(VER_TAG "${VER_TAG}-${VER_GIT_COUNT}")
endif()
git_describe(VER_GIT_DIRTY --always --dirty)
if("${VER_GIT_DIRTY}" MATCHES "-dirty")
    set(VER_TAG "${VER_TAG}-dirty")
endif()
message(STATUS "Version: ${VER_TAG}")

# config.h include path (SDL includes come from targets/shim)
include_directories(${PROJECT_BINARY_DIR})

if( "${CMAKE_TARGET_ARCHITECTURES}" MATCHES "i386" )
    if( MSVC )
        enable_language( ASM_MASM )
    else()
        enable_language( ASM )
    endif()
    set(can_use_assembler TRUE)
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)

# ---- Dependencies ----
if(WIN32 AND MSVC)
    # vcpkg/VS: CONFIG packages + imported targets
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(SDL2_mixer CONFIG REQUIRED)

    find_package(ZLIB REQUIRED)
    find_package(libzip CONFIG REQUIRED)

    find_package(Vorbis CONFIG REQUIRED)
    find_package(Ogg CONFIG REQUIRED)

    # SDL propagation shim:
    # Code uses #include <SDL.h> (SDL2-style), vcpkg provides headers in include/SDL2/SDL.h
    add_library(hypseus_sdl INTERFACE)
    target_include_directories(hypseus_sdl INTERFACE
        $<TARGET_PROPERTY:SDL2::SDL2,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:SDL2::SDL2,INTERFACE_INCLUDE_DIRECTORIES>/SDL2
    )
    target_link_libraries(hypseus_sdl INTERFACE SDL2::SDL2)
else()
    # Non-Windows: keep pkg-config behavior
    include(FindPkgConfig)

    pkg_search_module(SDL2 REQUIRED sdl2)
    pkg_search_module(SDL2_TTF REQUIRED SDL2_ttf)
    pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image)
    pkg_search_module(SDL2_MIXER REQUIRED SDL2_mixer)

    find_package(ZLIB REQUIRED)
    find_package(LibZip REQUIRED)
    find_package(Vorbis REQUIRED)
    find_package(Ogg REQUIRED)

    # Provide target-style wrappers for consistency
    add_library(hypseus_sdl INTERFACE)
    target_include_directories(hypseus_sdl INTERFACE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(hypseus_sdl INTERFACE ${SDL2_LIBRARIES})

    add_library(SDL2_ttf::SDL2_ttf INTERFACE IMPORTED)
    set_target_properties(SDL2_ttf::SDL2_ttf PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_TTF_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${SDL2_TTF_LIBRARIES}"
    )

    add_library(SDL2_image::SDL2_image INTERFACE IMPORTED)
    set_target_properties(SDL2_image::SDL2_image PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_IMAGE_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${SDL2_IMAGE_LIBRARIES}"
    )

    add_library(SDL2_mixer::SDL2_mixer INTERFACE IMPORTED)
    set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_MIXER_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${SDL2_MIXER_LIBRARIES}"
    )

    add_library(libzip::zip INTERFACE IMPORTED)
    set_target_properties(libzip::zip PROPERTIES
        INTERFACE_LINK_LIBRARIES "${LIBZIP_LIBRARY}"
    )
endif()

add_subdirectory(3rdparty)

add_subdirectory(cpu)
add_subdirectory(game)
add_subdirectory(manymouse)
add_subdirectory(io)
add_subdirectory(ldp-in)
add_subdirectory(ldp-out)
add_subdirectory(scoreboard)
add_subdirectory(sound)
add_subdirectory(timer)
add_subdirectory(video)
add_subdirectory(vldp)

# libmpeg2 dependency only when using ExternalProject build
if(NOT (WIN32 AND MSVC))
    add_dependencies(vldp libmpeg2)
endif()

add_dependencies(ldp-out vldp)
add_dependencies(game vldp)
add_dependencies(sound vldp)

# Ensure SDL include dirs are present for internal libs that include SDL.h
foreach(t
    timer cpu cpu_x86 game video sound io ldp-in scoreboard singeproxy
)
    if(TARGET ${t})
        target_link_libraries(${t} hypseus_sdl)
    endif()
endforeach()

if(APPLE)
    # Legacy compatibility; prefer targets elsewhere
    set(SDL2_LIBRARIES ${SDL2_LIBRARY})
endif()

set(HYPSEUS_SOURCES hypseus.cpp globals.h)
if(WIN32)
    list(APPEND HYPSEUS_SOURCES hypseus.rc)
endif()

add_executable(hypseus ${HYPSEUS_SOURCES})

# Main executable link
if(WIN32 AND MSVC)
    target_link_libraries(hypseus PRIVATE
        plog io timer sound video cpu game
        hypseus_sdl SDL2::SDL2main
        SDL2_ttf::SDL2_ttf
        SDL2_mixer::SDL2_mixer
        SDL2_image::SDL2_image
        Vorbis::vorbis Vorbis::vorbisfile Vorbis::vorbisenc
        Ogg::ogg
        ZLIB::ZLIB
        libzip::zip
    )
else()
    target_link_libraries(hypseus PRIVATE
        plog io timer sound video cpu game
        hypseus_sdl
        SDL2_ttf::SDL2_ttf
        SDL2_mixer::SDL2_mixer
        SDL2_image::SDL2_image
        ${Vorbis_Enc_LIBRARY} Vorbis::vorbis ${OGG_LIBRARIES} ${VORBISFILE_LIBRARIES} ${Vorbis_File_LIBRARY}
        ZLIB::ZLIB
        libzip::zip
    )
endif()

if(ASAN)
    if(NOT MSVC)
        target_link_options(hypseus PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    endif()
endif()

#[[
set_source_files_properties(testvldp.cpp PROPERTIES COMPILE_FLAGS -DSHOW_FRAMES)
add_executable(testvldp testvldp.cpp)
target_link_libraries(testvldp PRIVATE video vldp hypseus_sdl SDL2_ttf::SDL2_ttf SDL2_mixer::SDL2_mixer SDL2_image::SDL2_image)
]]

if(BUILDBOT)
    add_custom_command(TARGET hypseus POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:hypseus>
        COMMAND ${CMAKE_COMMAND} -E remove -f hypseus.zip
        COMMAND 7za a -mpass=4 -mfb=255 -y -tzip hypseus.zip $<TARGET_FILE:hypseus>
        COMMAND ${CMAKE_COMMAND} -E md5sum hypseus.zip > hypseus.md5
        COMMENT "Create zip and md5 for buildbot")
endif()

set(CPACK_PACKAGE_NAME "Hypseus Singe")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Hypseus Singe - Arcade Laserdisc Emulator")
set(CPACK_PACKAGE_VENDOR "DirtBagXon")
set(CPACK_PACKAGE_CONTACT "hypseus@users.noreply.github.com")
set(CPACK_PACKAGE_VERSION "${VER_TAG}")
set(CPACK_PACKAGE_VERSION_MAJOR "${VER_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VER_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VER_PATCH}")

include(CPack)
