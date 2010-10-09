
cmake_minimum_required(VERSION 2.6)

project(tanks)



option(ENABLE_GCC_PROFILING   "Enable -pg profiling(release only)" OFF)
option(ENABLE_DEV_FEATURES    "Enable development features" ON)
option(NO_ZLIB                "zlib / gzstream dependence" OFF)
option(UPDATE_TIMESTAMP       "Update build timestamp" OFF)
option(ENABLE_CWD             "Build with libcwd support" OFF)
option(DISABLE_NETWORK_OPTIMIZATIONS "Disable quantization etc to find CSP bugs" OFF)

option(ENABLE_ZB_MODE         "Enable ZB game mode" ON)
option(ENABLE_SOCCER_MODE     "Enable FMS game mode" OFF)

set (CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -Wall -Werror -O0 -ggdb --no-inline -D_DEBUG")

set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -I/home/musch/c++/release/include -L/home/musch/c++/release/lib")


if    (ENABLE_CWD)
set (CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}  -DLIBCWD_THREAD_SAFE -D_REENTRANT -DCWDEBUG -include libcwd/sys.h -include libcwd/debug.h")
endif (ENABLE_CWD)



add_definitions(-DTIXML_USE_STL)




if    (ENABLE_GCC_PROFILING)
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -p -pg")
endif (ENABLE_GCC_PROFILING)


if    (ENABLE_DEV_FEATURES)
add_definitions(-DENABLE_DEV_FEATURES)
endif (ENABLE_DEV_FEATURES)


if    (NO_ZLIB)
add_definitions(-DNO_ZLIB)
endif (NO_ZLIB)


if    (DISABLE_NETWORK_OPTIMIZATIONS)
add_definitions(-DDISABLE_NETWORK_OPTIMIZATIONS)
endif (DISABLE_NETWORK_OPTIMIZATIONS)


add_subdirectory(libs/gzstream)
add_subdirectory(libs/toolbox)
add_subdirectory(libs/bbmloader)
add_subdirectory(libs/master)
add_subdirectory(libs/ranking)
add_subdirectory(libs/network)

add_subdirectory(bluebeard)

add_subdirectory(games/tank)


add_subdirectory(tools/master_server EXCLUDE_FROM_ALL)

add_subdirectory(tools/modelviewer)
add_subdirectory(tools/particleviewer)

add_subdirectory(tools/autopatcher_client EXCLUDE_FROM_ALL)
add_subdirectory(tools/autopatcher_server EXCLUDE_FROM_ALL)

