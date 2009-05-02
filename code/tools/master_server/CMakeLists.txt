


add_executable       (master_server 
./src/main_master.cpp
./src/MasterServer.cpp
)


set (libs
toolbox master network
loki RakNet tinyxml
pthread # only for bsd compilation
)


if ( NOT NO_ZLIB)
set (libs ${libs} gzstream z)
endif (NOT NO_ZLIB)

if    (ENABLE_CWD)
set (libs ${libs} cwd_r)
endif (ENABLE_CWD)



target_link_libraries(master_server ${libs})


include_directories(
${tanks_SOURCE_DIR}/libs/toolbox/src
${tanks_SOURCE_DIR}/libs/master/src
${tanks_SOURCE_DIR}/libs/network/src
)

