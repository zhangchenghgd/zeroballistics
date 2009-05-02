
#ifndef TANK_BEACON_INCLUDED
#define TANK_BEACON_INCLUDED


#include "RigidBody.h"
#include "Datatypes.h"
#include "../Team.h"
#include "../HitpointTracker.h"
#include "Scheduler.h"



namespace physics
{
    class OdeSphereGeom;
}


//------------------------------------------------------------------------------
enum BEACON_STATE
{
    BS_FIXED,
    BS_DISPENSED,  // lying in beacon dispenser
    BS_UNDEPLOYED, // lying on ground
    BS_CARRIED,    // carried by tank
    BS_RISING,
    BS_HOVERING,   // risen into the air, but not yet deployed
    BS_DEPLOYED    // deployed and connected
};


//------------------------------------------------------------------------------
enum BEACON_EVENT
{
    BE_INSIDE_RADIUS_CHANGED = RBE_LAST,
    BE_DEPLOYED_CHANGED,
    BE_HOVERING_CHANGED, // client side only.
    BE_CARRIED_CHANGED,
    BE_HEALTHY,
    BE_UNDER_ATTACK,
    BE_HEALTH_CRITICAL
};


//------------------------------------------------------------------------------
/**
 *  Used to implement a hysteresis for health warnings. 
 */
enum BEACON_HEALTH_STATE
{
    BHS_HEALTHY,
    BHS_UNDER_ATTACK,
    BHS_CRITICAL
};

//------------------------------------------------------------------------------
class Beacon : public RigidBody, public HitpointTracker
{
 public:
    virtual ~Beacon();

    GAME_OBJECT_IMPL(Beacon);
    
    // used for object creation
    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp);

    // used for state value transmission 
    virtual void writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp);

    void setInsideRadius(bool a);
    bool isInsideRadius() const;

    BEACON_STATE getState() const;
    
    void setFixed();

    void setDeployed(bool s);
    bool isDeployed() const;
    
    void setTeamId(TEAM_ID t);
    TEAM_ID getTeamId() const;
    
    float getRadius() const;

    void setCarried(bool c);

    void rise();
    void stopRising(void*);
    void deploy(void*);
    
    void clearNeighbors();
    void addNeighbor(Beacon * b);
    std::vector<Beacon*> & getNeighbor();

    physics::OdeCCylinderGeom    * getRadiusGeom();
    physics::OdeSphereGeom       * getBodyGeom();
    const physics::OdeSphereGeom * getBodyGeom() const;

    virtual void setHitpoints(int hp, bool dirty_state = true);
    
    BEACON_HEALTH_STATE getHealthState() const;

    void handleHealth(float dt);

    void placeGeoms();

    void setNetworkFlagDirty();
    
    
 protected:
    Beacon();

    virtual void init();

    void setState(BEACON_STATE new_state);
    
    bool inside_radius_; ///< Whether this beacons is inside the area
                         ///of deployed beacons.

    
    TEAM_ID team_id_;

    BEACON_HEALTH_STATE health_state_;
    
    physics::OdeCCylinderGeom * radius_geom_; ///< Goes into beacon space, only used for boundary updates.
    physics::OdeSphereGeom    * body_geom_;   ///< Goes into beacon space, only used for boundary updates.


    hTask task_deploy_;
    
    std::vector<Beacon*> neighbor_;

    BEACON_STATE state_;
};

#endif
