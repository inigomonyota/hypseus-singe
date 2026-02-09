set( LIB_SOURCES
    cmdline.cpp conout.cpp error.cpp fileparse.cpp homedir.cpp input.cpp
    toolbox.cpp mpo_fileio.cpp parallel.cpp keycodes.cpp serialib.cpp
    network.cpp numstr.cpp sram.cpp zippp.cpp unzip.cpp
    
)

set( LIB_HEADERS
    cmdline.h conout.h configfile.h error.h fileparse.h homedir.h
    input.h limits.h mpo_fileio.h mpo_mem.h my_stdio.h keycodes.h
    serialib.h network.h numstr.h parallel.h sram.h unzip.h zippp.h
)

find_package(ZLIB REQUIRED)

# Try to find libzip using CMake config files first (vcpkg on Windows)
find_package(libzip QUIET CONFIG)

if(NOT libzip_FOUND)
    # Fallback to manual finding for systems without CMake config (Linux/Mac with pkg-config)
    find_path(LIBZIP_INCLUDE_DIR NAMES zip.h)
    mark_as_advanced(LIBZIP_INCLUDE_DIR)

    find_library(LIBZIP_LIBRARY NAMES zip)
    mark_as_advanced(LIBZIP_LIBRARY)

    if(LIBZIP_LIBRARY AND LIBZIP_INCLUDE_DIR)
        get_filename_component(_libzip_libdir ${LIBZIP_LIBRARY} DIRECTORY)
        find_file(_libzip_pkgcfg libzip.pc
            HINTS ${_libzip_libdir} ${LIBZIP_INCLUDE_DIR}/..
            PATH_SUFFIXES pkgconfig lib/pkgconfig libdata/pkgconfig
            NO_DEFAULT_PATH
        )

        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(
            LIBZIP
            REQUIRED_VARS
                LIBZIP_LIBRARY
                LIBZIP_INCLUDE_DIR
                _libzip_pkgcfg
        )

        set(LIBZIP_VERSION "unknown")
        if (_libzip_pkgcfg)
            file(READ "${_libzip_pkgcfg}" _libzip_pkgcfg_content)
            string(REGEX MATCH "Version: ([0-9]+\\.[0-9]+\\.[0-9]+)"
                _libzip_version_match "${_libzip_pkgcfg_content}")
            if (_libzip_version_match)
                string(REGEX REPLACE "Version: ([0-9]+\\.[0-9]+\\.[0-9]+)" "\\1"
                    LIBZIP_VERSION "${_libzip_version_match}")
            endif()
        endif()

        if (LIBZIP_FOUND)
            message(STATUS "libzip found: Version ${LIBZIP_VERSION}")

            if (LIBZIP_VERSION VERSION_LESS "1.3.0")
                message(FATAL_ERROR "- Version 1.3.0 or newer is required.")
            endif()

            if (NOT TARGET libzip::zip)
                add_library(libzip::zip UNKNOWN IMPORTED)
                set_target_properties(libzip::zip
                    PROPERTIES
                        INTERFACE_INCLUDE_DIRECTORIES ${LIBZIP_INCLUDE_DIR}
                        INTERFACE_LINK_LIBRARIES ZLIB::ZLIB
                        IMPORTED_LOCATION ${LIBZIP_LIBRARY}
                )
            endif()
        else()
            message(FATAL_ERROR "libzip not found")
        endif()
    else()
        message(FATAL_ERROR "libzip not found")
    endif()
else()
    message(STATUS "libzip found via CMake config")
endif()

if(NOT MSVC)
    set_source_files_properties( zippp.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations )
endif()

add_library( io ${LIB_SOURCES} ${LIB_HEADERS} )

# Use modern CMake targets when available
if(TARGET ZLIB::ZLIB)
    set(_ZLIB_TARGET ZLIB::ZLIB)
else()
    set(_ZLIB_TARGET ${ZLIB_LIBRARIES})
endif()

if(TARGET libzip::zip)
    set(_LIBZIP_TARGET libzip::zip)
else()
    set(_LIBZIP_TARGET ${LIBZIP_LIBRARY})
endif()

target_link_libraries( io ldp-out game plog manymouse ${_ZLIB_TARGET} ${_LIBZIP_TARGET})
if( WIN32 )
    target_link_libraries( io ws2_32 )
endif( WIN32 )
