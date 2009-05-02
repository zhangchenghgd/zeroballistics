#ifndef TANKGAME_MISSILE_INCLUDED
#define TANKGAME_MISSILE_INCLUDED


#include "Projectile.h"


namespace physics
{
    class CollisionInfo;
}

class Tank;

//------------------------------------------------------------------------------
class Missile : public Projectile
{
    
 public:

    virtual ~Missile();

    GAME_OBJECT_IMPL(Missile);

    virtual void frameMove(float dt);    

    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp);
    
    virtual void writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp);
    
    void setTank(Tank * tank);

    void initPosAndVelocity(const Matrix & t);
    
 protected:

    void updateTarget(float dt);

    void enableAiming(void*);
    void destroy(void*);
    void onTankDeleted();


    bool rayCollisionCallback(const physics::CollisionInfo & info);

    
    Missile();


    Tank * tank_; ///< We need to see where our tank is aiming

    Vector target_;
    float penetration_depth_;
    bool aiming_enabled_;

    
    float speed_;   ///< Parameters transferred to client for better visual appearance.
    float agility_;

    unsigned max_num_segments_;
    unsigned cur_ray_segment_; ///< For performance and correctness
                               ///reasons, don't test a single long
                               ///ray but split it into multiple
                               ///tests.
};


#endif
