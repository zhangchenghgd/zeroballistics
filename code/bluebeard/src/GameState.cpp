
#include "GameState.h"

#include <raknet/GetTime.h>

#include "physics/OdeSimulator.h"

#include "Log.h"
#include "NetworkCommandServer.h"
#include "Profiler.h"
#include "ParameterManager.h"

using namespace std;


const unsigned STATIC_QUAD_TREE_DEPTH = 6;


//------------------------------------------------------------------------------
GameState::GameState() :
    next_object_id_(0),
    simulator_(new physics::OdeSimulator("gamestate"))
{
}

//------------------------------------------------------------------------------
GameState::~GameState()
{
    reset();
}


//------------------------------------------------------------------------------
void GameState::reset()
{
    for (GameObjectContainer::const_iterator it=game_object_.begin();
         it != game_object_.end();
         ++it)
    {
        it->second->scheduleForDeletion();
        delete it->second;
    }    

    game_object_.clear();
    next_object_id_ = 0;

    // Must be deleted before simulator so geom cleanup code doesn't
    // cause a segfault...
    heightfield_geom_.reset(NULL);
}

//------------------------------------------------------------------------------
/**
 *  Takes the simulation forward one step.
 *
 *  Order is importants here: we want the GameObjects to have their
 *  interpolation done before letting the physics sim resolve
 *  collision constraints.
 */
void GameState::frameMove(float dt)
{
    PROFILE(GameState::frameMove);

    dt *= s_params.get<float>("physics.time_scale");
    
    
    GameObjectContainer::iterator it;
    for (it = game_object_.begin(); it != game_object_.end(); ++it)
    {
        it->second->frameMove(dt);
    }

    simulator_->frameMove(dt);
}



//------------------------------------------------------------------------------
/**
 *  Added objects will be deleted at gamestate destruction time.
 *
 *  \param object The object to add. If its id is
 *  INVALID_GAMEOBJECT_ID, the next free id is assigned to the
 *  object. Otherwise, the specified id is used.
 */
void GameState::addGameObject(GameObject * object)
{    
    if (object->getId() == INVALID_GAMEOBJECT_ID)
    {
        object->setId(getAndIncrementNextObjectId());
    } else
    {
        if (game_object_.find(object->getId()) != game_object_.end())
        {
            s_log << Log::warning
                  << "An already existing object was added to the gamestate: "
                  << *object
                  << ". Existing: "
                  << *game_object_.find(object->getId())
                  << ".\n";

            assert(false);
        }
    }    

    game_object_[object->getId()] = object;
}

//------------------------------------------------------------------------------
void GameState::deleteGameObject(uint16_t id)
{
    GameObjectContainer::iterator it = game_object_.find(id);

    if ( it == game_object_.end())
    {
        s_log << Log::warning
              << "Tried to delete nonexisting game object "
              << id << ".\n";

        return;
    }

    it->second->scheduleForDeletion();
    delete it->second;
    game_object_.erase(it);
}

//------------------------------------------------------------------------------
GameObject * GameState::getGameObject(uint16_t id)
{
    GameObjectContainer::iterator it = game_object_.find(id);
    
    if (it == game_object_.end()) return NULL;
    else                          return it->second;
}


//------------------------------------------------------------------------------
GameState::GameObjectContainer & GameState::getGameObjects()
{
    return game_object_;
}


//------------------------------------------------------------------------------
void GameState::setTerrainData(std::auto_ptr<terrain::TerrainData> data, unsigned collision_category)
{
    terrain_data_ = data;
    
    // Now that we now the dimensions of the terrain, we can create a
    // specialized quadtree for collision detection.
    Vector extents = Vector(terrain_data_->getHorzScale() * terrain_data_->getResX(),
                            terrain_data_->getMaxHeight() - terrain_data_->getMinHeight(),
                            terrain_data_->getHorzScale() * terrain_data_->getResZ());

    Vector center = Vector(extents.x_*0.5f,
                           (terrain_data_->getMaxHeight() + terrain_data_->getMinHeight()) * 0.5f,
                           extents.z_*0.5f);
  
    simulator_->getStaticSpace()->createQuadtreeSpace(center, extents, STATIC_QUAD_TREE_DEPTH);

    // After we have replaced the collision space, we can start
    // filling it with geoms...
    heightfield_geom_.reset(createHeightfieldGeom());
    heightfield_geom_->setCategory(collision_category, simulator_.get());
}

//------------------------------------------------------------------------------
const terrain::TerrainData * GameState::getTerrainData() const
{
    return terrain_data_.get();
}


//------------------------------------------------------------------------------
physics::OdeHeightfieldGeom * GameState::createHeightfieldGeom(physics::OdeCollisionSpace * space) const
{
    if (!terrain_data_.get()) return NULL;
    
    return new physics::OdeHeightfieldGeom(terrain_data_->getResX(), terrain_data_->getResZ(),
                                           terrain_data_->getHorzScale(),
                                           terrain_data_->getMinHeight(), terrain_data_->getMaxHeight(),
                                           terrain_data_->getHeightData(),
                                           space ? space : simulator_->getStaticSpace());
}


//------------------------------------------------------------------------------
physics::OdeSimulator * GameState::getSimulator()
{
    return simulator_.get();
}



//------------------------------------------------------------------------------
/**
 *  Returns the id the next added object should receive and increments
 *  the next_object_id_ counter.
 */
uint16_t GameState::getAndIncrementNextObjectId()
{
    return next_object_id_++;
}

