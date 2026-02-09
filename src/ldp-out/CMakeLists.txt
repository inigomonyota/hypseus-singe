set( LIB_SOURCES
    ldp.cpp
    framemod.cpp
    ldp-vldp.cpp
    ldp-vldp-audio.cpp
)


set( LIB_HEADERS
    framemod.h
    ldp.h
    ldp-vldp.h
)

find_package(Vorbis REQUIRED)
find_package(Ogg REQUIRED)

# On non-Apple systems, try to find vorbisfile via pkg-config
# On Windows/MSVC, this is handled by the FindVorbis module
IF(NOT APPLE AND NOT WIN32)
    PKG_SEARCH_MODULE(VORBISFILE REQUIRED vorbisfile)
ENDIF()

add_library( ldp-out ${LIB_SOURCES} ${LIB_HEADERS} )

# Use modern CMake targets when available
set(_VORBIS_LIBS)
if(TARGET Vorbis::vorbisfile)
    list(APPEND _VORBIS_LIBS Vorbis::vorbisfile)
elseif(VORBISFILE_LIBRARIES)
    list(APPEND _VORBIS_LIBS ${VORBISFILE_LIBRARIES})
elseif(Vorbis_File_LIBRARY)
    list(APPEND _VORBIS_LIBS ${Vorbis_File_LIBRARY})
endif()

target_link_libraries( ldp-out vldp game ${_VORBIS_LIBS} ${SDL2_LIBRARY} )
