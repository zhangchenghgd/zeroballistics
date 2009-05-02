
#ifndef TANK_GAMESTATE_INCLUDED
#define TANK_GAMESTATE_INCLUDED

#include <map>
#include <memory>

#include <raknet/RakNetTypes.h>

#include "Datatypes.h"
#include "PlayerInput.h"
#include "TerrainData.h"

namespace physics
{
    class OdeSimulator;
    class OdeCollisionSpace;
    class OdeHeightfieldGeom;
}

class GameObject;

//------------------------------------------------------------------------------
class GameState
{
 public:

    typedef std::map<uint16_t,GameObject*> GameObjectContainer;

    GameState();
    ~GameState();

    void reset();
    
    void frameMove(float dt);

    void addGameObject(GameObject * object);
    void deleteGameObject(uint16_t id);
    GameObject * getGameObject(uint16_t id);
    GameObjectContainer & getGameObjects();

    void setTerrainData(std::auto_ptr<terrain::TerrainData> data, unsigned collision_category);
    const terrain::TerrainData * getTerrainData() const;
    physics::OdeHeightfieldGeom * createHeightfieldGeom(physics::OdeCollisionSpace * space = NULL) const;
    
    physics::OdeSimulator * getSimulator();

    uint16_t getAndIncrementNextObjectId();
    
 protected:
    
    uint16_t next_object_id_; ///< Server only: the id of the next
                              ///gameobject.

    GameObjectContainer game_object_;

    std::auto_ptr<const terrain::TerrainData> terrain_data_;
    std::auto_ptr<physics::OdeHeightfieldGeom> heightfield_geom_;
    
    const std::auto_ptr<physics::OdeSimulator> simulator_;
};

#endif // TANK_GAMESTATE_INCLUDED
