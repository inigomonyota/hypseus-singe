set( LIB_SOURCES
    video.cpp
    tms9128nl.cpp
    SDL_FontCache.c
    led.cpp
    palette.cpp
    splash.cpp
    rgb2yuv.cpp
)

# Add assembly file based on platform and architecture
if( "${CMAKE_TARGET_ARCHITECTURES}" MATCHES "i386" )
    if( MSVC )
        list( APPEND LIB_SOURCES rgb2yuv-masm.asm )
    else()
        list( APPEND LIB_SOURCES rgb2yuv-gas.s )
    endif()
endif()

set( LIB_HEADERS
    led.h
    palette.h
    rgb2yuv.h
    SDL_FontCache.h
    tms9128nl.h
    video.h
    splash.h
    yuv2rgb_lookup.h
)

if(NOT MSVC)
    set_source_files_properties(SDL_FontCache.c PROPERTIES COMPILE_FLAGS -Wno-maybe-uninitialized)
    set_source_files_properties(video.cpp PROPERTIES COMPILE_FLAGS -Wno-unused-const-variable)
endif()

add_library( video ${LIB_SOURCES} ${LIB_HEADERS} )
target_link_libraries( video plog )
