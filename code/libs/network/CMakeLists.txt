

include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src)


set(networkSources
./src/VersionInfo.cpp
./src/VersionHandshakePlugin.cpp
./src/MultipleConnectPlugin.cpp
./src/NetworkUtils.cpp
./src/ServerInterface.cpp
./src/ClientInterface.cpp
)

add_library(network ${networkSources} )

