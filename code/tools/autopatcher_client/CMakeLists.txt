


add_executable       (autopatcher
./src/main.cpp
./src/PatcherApp.cpp
)


set (libs
network toolbox 
RakNet
tinyxml_static gzstream_static 
loki_static libz.a
FOX-1.6
X11 Xext pthread rt
)

target_link_libraries(autopatcher ${libs})


include_directories(
${tanks_SOURCE_DIR}/libs/network/src
${tanks_SOURCE_DIR}/libs/toolbox/src
)