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

if(NOT APPLE AND NOT WIN32)
  find_package(PkgConfig REQUIRED)
  pkg_search_module(VORBISFILE REQUIRED vorbisfile)
endif()

if(WIN32 AND MSVC)
    find_package(ogg CONFIG REQUIRED)
    find_package(vorbis CONFIG REQUIRED)
else()
    find_package(Ogg REQUIRED)
    find_package(Vorbis REQUIRED)
endif()

add_library(ldp-out ${LIB_SOURCES} ${LIB_HEADERS})

target_link_libraries(ldp-out PRIVATE
  vldp game
  $<$<BOOL:${WIN32}>:Vorbis::vorbisfile;Ogg::ogg>
  $<$<NOT:$<BOOL:${WIN32}>>:${VORBISFILE_LIBRARIES}>
  ${SDL2_LIBRARY}
)
