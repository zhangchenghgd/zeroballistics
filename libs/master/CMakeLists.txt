
include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src)


set(masterSources
./src/MasterServerRequest.cpp
./src/MasterServerPunchthrough.cpp
./src/MasterServerRegistrator.cpp
./src/ServerInfo.cpp
./src/ServerList.cpp
)


add_library(master ${masterSources} )

