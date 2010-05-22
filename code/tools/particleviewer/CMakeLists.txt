

set (particle_viewer_libs

bluebeard bbmloader toolbox master

SDL GL GLU gzstream z loki RakNet openal alut vorbisfile ode tinyxml

osg osgDB osgUtil osgText osgParticle osgViewer

CEGUIBase CEGUIOpenGLRenderer

boost_filesystem
)

if    (ENABLE_DEV_FEATURES)
set (particle_viewer_libs ${particle_viewer_libs} glut)
endif (ENABLE_DEV_FEATURES)




include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src
                    ${tanks_SOURCE_DIR}/libs/bbmloader/src
                    ${tanks_SOURCE_DIR}/bluebeard/src
                    /usr/local/include/CEGUI)



set(particleViewerSources
./src/main_viewer.cpp
./src/ParticleViewer.cpp
)

add_executable       (particle_viewer ${particleViewerSources})
target_link_libraries(particle_viewer ${particle_viewer_libs})

