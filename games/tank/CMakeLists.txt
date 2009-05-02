




set (full_libs

bluebeard bbmloader toolbox master

SDL GL GLU loki raknet openal alut vorbisfile ode tinyxml

osg osgDB osgUtil osgText osgParticle osgViewer

CEGUIBase CEGUIOpenGLRenderer

boost_filesystem
)


if    (ENABLE_DEV_FEATURES)
set (full_libs ${full_libs} glut)
endif (ENABLE_DEV_FEATURES)

if (NOT NO_ZLIB)
set (full_libs ${full_libs} gzstream z)
endif (NOT NO_ZLIB)

if    (ENABLE_CWD)
set (full_libs ${full_libs} cwd_r)
endif (ENABLE_CWD)


set (dedicated_libs

toolbox master

loki raknet ode tinyxml 

boost_filesystem

pcre
)


if    (ENABLE_CWD)
set (dedicated_libs ${dedicated_libs} cwd_r)
endif (ENABLE_CWD)


if( NOT NO_ZLIB)
set (dedicated_libs ${dedicated_libs} gzstream z)
endif (NOT NO_ZLIB)



set(tankSources
./src/main_client.cpp 
./src/Version.cpp 
./src/TankAppClient.cpp 
./src/Tank.cpp 
./src/HitpointTracker.cpp 
./src/Projectile.cpp 
./src/GameLogicServerCommon.cpp 
./src/GameLogicClientCommon.cpp 
./src/GameLogicServerDeathmatch.cpp 
./src/GameLogicClientDeathmatch.cpp 
./src/GameLogicServerTeamDeathmatch.cpp 
./src/GameLogicClientTeamDeathmatch.cpp 
./src/GameHudTank.cpp 
./src/GUITeaser.cpp
./src/MainMenu.cpp 
./src/Score.cpp 
./src/Team.cpp 
./src/GUIServerBrowser.cpp 
./src/GUIScore.cpp 
./src/GUITankEquipment.cpp 
./src/GUIUpgradeSystem.cpp 
./src/GUITeamSelect.cpp 
./src/GUIHelp.cpp 
./src/Minimap.cpp 
./src/WeaponSystem.cpp 
./src/TankCannon.cpp 
./src/TankMine.cpp 
./src/TankMineDispenser.cpp 
./src/TankMachineGun.cpp 
./src/GUIOptions.cpp 
./src/GUIHostMenu.cpp 
./src/TankVisual.cpp 
./src/TankMineVisual.cpp 
./src/ProjectileVisual.cpp 
./src/SpawnPos.cpp 
./src/SpectatorCamera.cpp 
./src/IntegratedServer.cpp


./src/beaconstrike/Beacon.cpp 
./src/beaconstrike/GameLogicServerBeaconstrike.cpp 
./src/beaconstrike/GameLogicClientBeaconstrike.cpp 
./src/beaconstrike/BeaconBoundary.cpp 
./src/beaconstrike/BeaconBoundaryClient.cpp 
./src/beaconstrike/BeaconBoundaryServer.cpp 
./src/beaconstrike/TeamBs.cpp 
./src/beaconstrike/BeaconVisual.cpp 
./src/beaconstrike/SpawnStageServer.cpp 
./src/beaconstrike/SpawnStageClient.cpp 
)


set(serverSources
./src/main_server.cpp 
./src/TankAppServer.cpp 
./src/HitpointTracker.cpp 
./src/Tank.cpp 
./src/Projectile.cpp 
./src/GameLogicServerCommon.cpp 
./src/GameLogicServerDeathmatch.cpp 
./src/GameLogicServerTeamDeathmatch.cpp 
./src/Team.cpp 
./src/Score.cpp 
./src/WeaponSystem.cpp 
./src/TankCannon.cpp 
./src/TankMineDispenser.cpp 
./src/TankMine.cpp
./src/TankMachineGun.cpp 
./src/TankVisual.cpp 
./src/ProjectileVisual.cpp 
./src/TankMineVisual.cpp 
./src/SpawnPos.cpp 


./src/beaconstrike/Beacon.cpp 
./src/beaconstrike/GameLogicServerBeaconstrike.cpp 
./src/beaconstrike/BeaconBoundary.cpp 
./src/beaconstrike/BeaconBoundaryServer.cpp 
./src/beaconstrike/TeamBs.cpp 
./src/beaconstrike/BeaconVisual.cpp 
./src/beaconstrike/SpawnStageServer.cpp 
)




set(serverDedSources
./src/main_server_ded.cpp
./src/Version.cpp 
./src/Tank.cpp 
./src/HitpointTracker.cpp 
./src/Projectile.cpp 
./src/GameLogicServerCommon.cpp 
./src/GameLogicServerDeathmatch.cpp 
./src/GameLogicServerTeamDeathmatch.cpp 
./src/Team.cpp 
./src/Score.cpp 
./src/WeaponSystem.cpp 
./src/TankCannon.cpp 
./src/TankMineDispenser.cpp 
./src/TankMine.cpp
./src/TankMachineGun.cpp 
./src/SpawnPos.cpp 


./src/beaconstrike/TeamBs.cpp 
./src/beaconstrike/Beacon.cpp 
./src/beaconstrike/BeaconBoundary.cpp 
./src/beaconstrike/BeaconBoundaryServer.cpp 
./src/beaconstrike/GameLogicServerBeaconstrike.cpp 
./src/beaconstrike/SpawnStageServer.cpp 


${tanks_SOURCE_DIR}/bluebeard/src/RegEx.cpp
${tanks_SOURCE_DIR}/bluebeard/src/NetworkServer.cpp
${tanks_SOURCE_DIR}/bluebeard/src/Water.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/NetworkCommandServer.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/NetworkCommandClient.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/NetworkCommand.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/GameState.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/PlayerInput.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/GameObject.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/PuppetMasterServer.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/ServerAnnouncer.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/Controllable.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/ServerPlayer.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/Player.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/GameLogicServer.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/RigidBody.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/physics/OdeRigidBody.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/physics/OdeCollision.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/physics/OdeSimulator.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/physics/OdeModelLoader.cpp 
${tanks_SOURCE_DIR}/bluebeard/src/physics/OdeCollisionSpace.cpp

${tanks_SOURCE_DIR}/bluebeard/src/TerrainData.cpp

${tanks_SOURCE_DIR}/libs/bbmloader/src/LevelData.cpp


)


include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src
                    ${tanks_SOURCE_DIR}/libs/bbmloader/src
                    ${tanks_SOURCE_DIR}/libs/master/src
                    ${tanks_SOURCE_DIR}/bluebeard/src
                    /usr/local/include/CEGUI)





add_executable       (tank ${tankSources})
target_link_libraries(tank ${full_libs})
add_dependencies     (tank bluebeard toolbox bbmloader)

add_executable       (server EXCLUDE_FROM_ALL ${serverSources})
target_link_libraries(server ${full_libs})
add_dependencies     (server bluebeard toolbox bbmloader)


add_executable       (server_ded ${serverDedSources})
target_link_libraries(server_ded ${dedicated_libs})

SET_TARGET_PROPERTIES(server_ded PROPERTIES COMPILE_FLAGS -DDEDICATED_SERVER)
