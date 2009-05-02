
#ifndef TANK_TEAM_BS_INCLUDED
#define TANK_TEAM_BS_INCLUDED

#include "../Team.h"
#include "RegisteredFpGroup.h"


namespace physics
{
    class OdeCollisionSpace;
}

class BeaconBoundaryClient;



const TEAM_ID TEAM_ID_ATTACKER = 0;
const TEAM_ID TEAM_ID_DEFENDER = 1;


const unsigned NUM_TEAMS_BS = 2;
const std::string TEAM_CONFIG_BS[] = { "team_attacker", "team_defender" };


//------------------------------------------------------------------------------
class ServerTeam : public Team
{
 public:
    ServerTeam();
    virtual ~ServerTeam();

    void reset();
    
    std::string getTankName() const;
    std::string getBeaconName() const;

    bool getBeaconFromQueue();
    void fillBeaconQueue(float amount);
    
 protected:
    float beacon_queue_;

    RegisteredFpGroup fp_group_;
};

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
class ClientTeam : public Team
{
 public:
    ClientTeam();
    virtual ~ClientTeam();
    
    void createBoundary(physics::OdeCollisionSpace * world_space);

    BeaconBoundaryClient * getBoundary();

 protected:

    BeaconBoundaryClient * beacon_boundary_;
};

#endif

#endif
