
#ifndef TANK_SPAWN_STAGE_SERVER_INCLUDED
#define TANK_SPAWN_STAGE_SERVER_INCLUDED


#include "TeamBs.h"
#include "Matrix.h"


class LocalParameters;

class Beacon;
class RigidBody;
class SpawnPos;

namespace physics
{
    class OdeGeom;
    struct CollisionInfo;
}



//------------------------------------------------------------------------------
class SpawnStageServer
{
 public:
    SpawnStageServer();
    virtual ~SpawnStageServer();

    TEAM_ID getOwnerTeam() const;
    
    void setDefenderBeacon(Beacon * b);
    Beacon * getDefenderBeacon();

    void addPlayerSpawnPos(SpawnPos * pos, TEAM_ID team);
    void setBeaconSpawnPos(std::auto_ptr<SpawnPos> pos);

    const SpawnPos * getBeaconSpawnPos() const;

    const std::vector<SpawnPos*> & getSpawnPositions(TEAM_ID team);
    
 protected:

    Beacon * defender_beacon_; ///< Conquerable beacon. Is set to NULL
                               ///when stage is conquered.
    
    /// Need a pointer here because we register a collision callback...    
    std::vector<SpawnPos*> player_spawn_pos_[NUM_TEAMS_BS]; 
    std::auto_ptr<SpawnPos> beacon_spawn_pos_;
};


#endif 
