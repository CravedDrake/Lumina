# FindLibVLC.cmake
#
# Locates libVLC (the 3.x C API) and creates the imported target LibVLC::LibVLC.
#
# Search order:
#   1. LIBVLC_ROOT (CMake variable or environment variable) containing
#        include/vlc/vlc.h   and   lib/libvlc.(so|lib|dylib)
#      This is how the Windows "VLC SDK" (the `sdk/` folder of the VLC zip
#      package) is consumed.
#   2. pkg-config (`libvlc`), the normal route on Linux.
#
# Result variables: LibVLC_FOUND, LibVLC_VERSION (if known),
#                   LIBVLC_INCLUDE_DIR, LIBVLC_LIBRARY

if(NOT LIBVLC_ROOT AND DEFINED ENV{LIBVLC_ROOT})
    set(LIBVLC_ROOT "$ENV{LIBVLC_ROOT}")
endif()

if(LIBVLC_ROOT)
    find_path(LIBVLC_INCLUDE_DIR vlc/vlc.h
        HINTS "${LIBVLC_ROOT}/include" "${LIBVLC_ROOT}/sdk/include" NO_DEFAULT_PATH)
    find_library(LIBVLC_LIBRARY NAMES vlc libvlc
        HINTS "${LIBVLC_ROOT}/lib" "${LIBVLC_ROOT}/sdk/lib" NO_DEFAULT_PATH)
    find_library(LIBVLCCORE_LIBRARY NAMES vlccore libvlccore
        HINTS "${LIBVLC_ROOT}/lib" "${LIBVLC_ROOT}/sdk/lib" NO_DEFAULT_PATH)
endif()

if(NOT LIBVLC_INCLUDE_DIR OR NOT LIBVLC_LIBRARY)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_LIBVLC QUIET libvlc)
        set(LibVLC_VERSION "${PC_LIBVLC_VERSION}")
    endif()
    find_path(LIBVLC_INCLUDE_DIR vlc/vlc.h HINTS ${PC_LIBVLC_INCLUDE_DIRS})
    find_library(LIBVLC_LIBRARY NAMES vlc libvlc HINTS ${PC_LIBVLC_LIBRARY_DIRS})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibVLC
    REQUIRED_VARS LIBVLC_LIBRARY LIBVLC_INCLUDE_DIR
    VERSION_VAR LibVLC_VERSION)

if(LibVLC_FOUND AND NOT TARGET LibVLC::LibVLC)
    add_library(LibVLC::LibVLC UNKNOWN IMPORTED)
    set_target_properties(LibVLC::LibVLC PROPERTIES
        IMPORTED_LOCATION "${LIBVLC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBVLC_INCLUDE_DIR}")
endif()
mark_as_advanced(LIBVLC_INCLUDE_DIR LIBVLC_LIBRARY LIBVLCCORE_LIBRARY)
