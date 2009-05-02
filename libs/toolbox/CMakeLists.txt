

set(toolboxSources
./src/ConsoleApp.cpp 
./src/Bitmap.cpp 
./src/Console.cpp 
./src/Datatypes.cpp 
./src/Exception.cpp 
./src/Frustum.cpp 
./src/Geometry.cpp 
./src/Log.cpp 
./src/Matrix.cpp 
./src/Observable.cpp 
./src/Plane.cpp 
./src/Quaternion.cpp 
./src/Scheduler.cpp 
./src/TimeStructs.cpp 
./src/Tokenizer.cpp 
./src/utility_Math.cpp 
./src/Utils.cpp 
./src/VariableWatcher.cpp 
./src/Vector2d.cpp 
./src/Vector.cpp 
./src/Serializer.cpp 
./src/Profiler.cpp 
./src/ParameterManager.cpp
./src/Entity.cpp
./src/RegisteredFpGroup.cpp
./src/TinyXmlUtils.cpp 
./src/md5.cpp
)


add_library(toolbox ${toolboxSources} )


if (UPDATE_TIMESTAMP)
add_custom_target(timestamp ALL 
                  COMMAND touch Log.cpp
                  WORKING_DIRECTORY ${tanks_SOURCE_DIR}/libs/toolbox/src
                  COMMENT "Touching Log.cpp"
                  VERBATIM)
endif (UPDATE_TIMESTAMP)

add_dependencies(toolbox timestamp)