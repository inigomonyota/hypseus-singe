set(LIB_SOURCES
    vldp.cpp
    vldp_internal.cpp
    mpegscan.cpp
)

set(LIB_HEADERS
    mpegscan.h
    vldp_common.h
    vldp.h
    vldp_internal.h
)

if(VLDP_DEBUG)
    set_source_files_properties(${LIB_SOURCES} PROPERTIES COMPILE_DEFINITIONS "VLDP_DEBUG")
endif()

add_library(vldp ${LIB_SOURCES} ${LIB_HEADERS})
target_link_libraries(vldp PRIVATE plog)

# ---- MPEG2 (Windows/MSVC via vcpkg; non-Windows via existing vars) ----
if(WIN32 AND MSVC)
    # Prefer vcpkg CONFIG packages. If these targets don't exist in your vcpkg snapshot,
    # leave this block and use the manual find_path/find_library shim from the top-level.
    #
    # Typical target names in vcpkg for libmpeg2 are NOT consistent, so this is defensive.
    find_path(LIBMPEG2_INCLUDE_DIR NAMES mpeg2dec/mpeg2.h)
    find_library(LIBMPEG2_LIBRARY NAMES mpeg2 mpeg2dec libmpeg2)

    if(NOT LIBMPEG2_INCLUDE_DIR OR NOT LIBMPEG2_LIBRARY)
        message(FATAL_ERROR "MPEG2 not found for vldp. Install/update vcpkg port providing libmpeg2, or provide manual paths.")
    endif()

    target_include_directories(vldp PRIVATE ${LIBMPEG2_INCLUDE_DIR})
    target_link_libraries(vldp PRIVATE ${LIBMPEG2_LIBRARY})
else()
    # Existing (pkg-config / legacy) variables
    target_include_directories(vldp PRIVATE ${MPEG2_INCLUDE_DIRS})
    target_link_libraries(vldp PRIVATE ${MPEG2_LIBRARIES})
endif()
