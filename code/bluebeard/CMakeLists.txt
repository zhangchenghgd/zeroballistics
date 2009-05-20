

include_directories(${tanks_SOURCE_DIR}/libs/toolbox/src
                    ${tanks_SOURCE_DIR}/libs/gzstream/src
                    ${tanks_SOURCE_DIR}/libs/bbmloader/src
                    ${tanks_SOURCE_DIR}/libs/network/src
                    ${tanks_SOURCE_DIR}/libs/ranking/src
                    ${tanks_SOURCE_DIR}/libs/master/src
                    ${tanks_SOURCE_DIR}/bluebeard/src
                    /usr/local/include/CEGUI)



set(bluebeardSources
./src/ObjectParts.cpp 
./src/RegEx.cpp 
./src/Camera.cpp 
./src/NetworkServer.cpp 
./src/SdlApp.cpp 
./src/GUIConsole.cpp 
./src/GUIProfiler.cpp 
./src/NetworkCommandServer.cpp 
./src/NetworkCommandClient.cpp 
./src/NetworkCommand.cpp 
./src/GameState.cpp 
./src/PlayerInput.cpp 
./src/GameObject.cpp 
./src/PuppetMasterClient.cpp 
./src/PuppetMasterServer.cpp 
./src/ServerAnnouncer.cpp 
./src/Controllable.cpp 
./src/ClientPlayer.cpp 
./src/ServerPlayer.cpp 
./src/Player.cpp 
./src/GameLogicClient.cpp 
./src/GameLogicServer.cpp 
./src/RigidBody.cpp 
./src/SceneManager.cpp 
./src/Shadow.cpp 
./src/ParticleManager.cpp 
./src/ParticleCustomOperator.cpp 
./src/ParticleCustomShooter.cpp 
./src/ParticleCustomPlacer.cpp 
./src/EffectManager.cpp 
./src/VariableWatcherVisual.cpp 
./src/Gui.cpp 
./src/GuiLogger.cpp 
./src/MetaTask.cpp 
./src/HudNotificationArea.cpp 
./src/GameHud.cpp 
./src/HudTextElement.cpp 
./src/HudTextureElement.cpp 
./src/HudBar.cpp 
./src/HudAlphaBar.cpp 
./src/SoundManager.cpp 
./src/SoundSource.cpp 
./src/SoundBuffer.cpp 
./src/TextureManager.cpp 
./src/ParticleEffect.cpp 
./src/GameObjectVisual.cpp 
./src/RigidBodyVisual.cpp 
./src/ControllableVisual.cpp 
./src/InputHandler.cpp 
./src/UtilsOsg.cpp 
./src/InstancedGeometry.cpp 
./src/InstancePlacer.cpp 

./src/WaterVisual.cpp 
./src/Water.cpp 
./src/TerrainVisual.cpp
./src/TerrainData.cpp
./src/TerrainDataClient.cpp
./src/ClipmapGrid.cpp
./src/ToroidalBuffer.cpp
./src/BufferObject.cpp
./src/OsgNodeWrapper.cpp
./src/LodUpdater.cpp

./src/OggStream.cpp
./src/OggVorbis.cpp 

./src/Paths.cpp
./src/UserPreferences.cpp

./src/BbmOsgConverter.cpp 
./src/ReaderWriterBbm.cpp 

./src/WaypointManagerClient.cpp
./src/WaypointManagerServer.cpp
./src/AIPlayer.cpp

./src/physics/OdeRigidBody.cpp 
./src/physics/OdeCollision.cpp 
./src/physics/OdeSimulator.cpp 
./src/physics/OdeModelLoader.cpp 
./src/physics/OdeCollisionSpace.cpp
)

add_library(bluebeard ${bluebeardSources} )
