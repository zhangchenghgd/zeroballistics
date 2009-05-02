


add_executable       (autopatcher_server
./src/main.cpp
./src/PatcherServer.cpp
)


set (libs
toolbox network
loki raknet tinyxml
gzstream z
mysqlclient
raknet 
)


target_link_libraries(autopatcher_server ${libs})


include_directories(
${tanks_SOURCE_DIR}/libs/toolbox/src
${tanks_SOURCE_DIR}/libs/network/src
)

