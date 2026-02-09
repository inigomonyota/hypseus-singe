# - Find ogg
# Find the native ogg includes and libraries
#
#  OGG_INCLUDE_DIRS - where to find ogg.h, etc.
#  OGG_LIBRARIES    - List of libraries when using ogg.
#  OGG_FOUND        - True if ogg found.

# First try to find Ogg using CMake config (vcpkg provides this)
find_package(Ogg QUIET CONFIG)

if(Ogg_FOUND OR OGG_FOUND)
	# Found via CMake config, Ogg::ogg target should be available
	if(NOT TARGET Ogg::ogg AND TARGET Ogg)
		# Some configs define 'Ogg' instead of 'Ogg::ogg'
		add_library(Ogg::ogg ALIAS Ogg)
	endif()
	message(STATUS "Ogg found via CMake config")
	return()
endif()

# Fallback to manual finding for systems without CMake config
if (OGG_INCLUDE_DIR)
	# Already in cache, be silent
	set(OGG_FIND_QUIETLY TRUE)
endif ()

find_package (PkgConfig QUIET)
pkg_check_modules (PC_OGG QUIET ogg>=1.3.0)

set (OGG_VERSION ${PC_OGG_VERSION})

find_path (OGG_INCLUDE_DIR ogg/ogg.h
	HINTS
		${PC_OGG_INCLUDEDIR}
		${PC_OGG_INCLUDE_DIRS}
		${OGG_ROOT}
	)
# MSVC built ogg may be named ogg_static.
# The provided project files name the library with the lib prefix.
find_library (OGG_LIBRARY
	NAMES
    libogg.a
		ogg
		ogg_static
		libogg
		libogg_static
	HINTS
		${PC_OGG_LIBDIR}
		${PC_OGG_LIBRARY_DIRS}
		${OGG_ROOT}
	)
# Handle the QUIETLY and REQUIRED arguments and set OGG_FOUND
# to TRUE if all listed variables are TRUE.
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (Ogg
	REQUIRED_VARS
		OGG_LIBRARY
		OGG_INCLUDE_DIR
	VERSION_VAR
		OGG_VERSION
	)

if (OGG_FOUND)
	set (OGG_LIBRARIES ${OGG_LIBRARY})
	set (OGG_INCLUDE_DIRS ${OGG_INCLUDE_DIR})
	
	if(NOT TARGET Ogg::ogg)
	add_library(Ogg::ogg UNKNOWN IMPORTED)
		set_target_properties(Ogg::ogg PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${OGG_INCLUDE_DIRS}"
			IMPORTED_LOCATION "${OGG_LIBRARIES}"
		)
  endif ()
endif ()

mark_as_advanced (OGG_INCLUDE_DIR OGG_LIBRARY)