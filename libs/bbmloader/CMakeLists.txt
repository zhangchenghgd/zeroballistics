

include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src)


set(bbmloaderSources
./src/BbmImporter.cpp
./src/LevelData.cpp
)

add_library(bbmloader ${bbmloaderSources} )
