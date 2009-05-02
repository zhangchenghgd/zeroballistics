

set (model_viewer_libs

bluebeard bbmloader toolbox master

SDL GL GLU gzstream z loki raknet openal alut vorbisfile ode tinyxml

osg osgDB osgUtil osgText osgParticle osgViewer

CEGUIBase CEGUIOpenGLRenderer

boost_filesystem
)

if    (ENABLE_DEV_FEATURES)
set (model_viewer_libs ${model_viewer_libs} glut)
endif (ENABLE_DEV_FEATURES)




include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src
                    ${tanks_SOURCE_DIR}/libs/bbmloader/src
                    ${tanks_SOURCE_DIR}/bluebeard/src
                    /usr/local/include/CEGUI)



set(modelViewerSources
./src/main_viewer.cpp
./src/ModelViewer.cpp

)

add_executable       (model_viewer EXCLUDE_FROM_ALL ${modelViewerSources})
target_link_libraries(model_viewer ${model_viewer_libs})

