

project(tanks)



option(ENABLE_PROFILING       "Enable -pg profiling(release only)" OFF)
option(ENABLE_DEV_FEATURES    "Enable development features" ON)
option(NO_ZLIB                "zlib / gzstream dependence" OFF)
option(UPDATE_TIMESTAMP       "Update build timestamp" OFF)


set (CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -Wall -Werror -O0 -ggdb --no-inline -D_DEBUG")

set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -I/home/musch/c++/release/include -L/home/musch/c++/release/lib")



add_definitions(-DTIXML_USE_STL)




if    (ENABLE_PROFILING)
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -p -pg")
endif (ENABLE_PROFILING)


if    (ENABLE_DEV_FEATURES)
add_definitions(-DENABLE_DEV_FEATURES)
endif (ENABLE_DEV_FEATURES)


if    (NO_ZLIB)
add_definitions(-DNO_ZLIB)
endif (NO_ZLIB)








add_subdirectory(libs/toolbox)
add_subdirectory(libs/bbmloader)
add_subdirectory(libs/master)

add_subdirectory(bluebeard)

add_subdirectory(games/tank)


add_subdirectory(tools/master_server)
add_subdirectory(tools/modelviewer)
add_subdirectory(tools/particleviewer)
