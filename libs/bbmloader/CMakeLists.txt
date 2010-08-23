

include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src
                    ${tanks_SOURCE_DIR}/libs/gzstream/src)


set(bbmloaderSources
./src/BbmImporter.cpp
./src/NvMeshMender.cpp
./src/LevelData.cpp
)

add_library(bbmloader ${bbmloaderSources} )
