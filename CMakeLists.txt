project(DVSWITCH CXX C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
include(FindPkgConfig)
pkg_check_modules(GTKMM REQUIRED gtkmm-2.4)
pkg_check_modules(LIBAVCODEC REQUIRED libavcodec)

# Cope with recent move of ffmpeg headers into subdirectories
set(LIBAVCODEC_INCLUDE_DIRS ${LIBAVCODEC_INCLUDE_DIRS} ${LIBAVCODEC_INCLUDEDIR}/libavcodec)

set(prefix /usr/local
    CACHE STRING "Installation prefix")
set(bindir ${prefix}/bin
    CACHE STRING "Executable installation directory")
set(sharedir ${prefix}/share
    CACHE STRING "Shared data installation directory")
set(docdir ${sharedir}/doc
    CACHE STRING "Documentation installation directory")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -Wall -Wextra")
add_definitions(-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_POSIX_C_SOURCE)
include_directories(src)

# Remove bogus -fPIC (Debian bug #478404)
set(CMAKE_SHARED_LIBRARY_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "")

add_subdirectory(data)
add_subdirectory(src)
add_subdirectory(tests)

install(FILES doc/README ChangeLog
        DESTINATION ${docdir}/dvswitch)
