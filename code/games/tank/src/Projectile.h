#ifndef TANKGAME_PROJECTILE_INCLUDED
#define TANKGAME_PROJECTILE_INCLUDED


#include <string>

#include "RigidBody.h"


class GameLogicServerCommon;

namespace RakNet
{
    class BitStream;
}

namespace physics
{
    struct CollisionInfo;
}

//------------------------------------------------------------------------------
/**
 *  We can only select the right collision event after we have seen
 *  all of them. The currently best (first) event for projectiles
 *  colliding is stored here. When the projectile generates
 *  IN_PROGRESS events, the best contact is used to determine what was
 *  hit actually.
 */
struct ProjectileCollisionInfo
{
    uint16_t game_object_id_; ///< Work with id here in case object is deleted.
    physics::CollisionInfo info_;

    ProjectileCollisionInfo() :
        game_object_id_(INVALID_GAMEOBJECT_ID)
    {}
};

//------------------------------------------------------------------------------
class Projectile : public RigidBody
{
    
 public:

    virtual ~Projectile();

    GAME_OBJECT_IMPL(Projectile);

    void setSection(const std::string & s);
    const std::string & getSection() const;

    void setAdditionalDamage(unsigned dam);

    float getSplashRadius()  const;
    float getDamage()        const;
    float getImpactImpulse() const;
    
    virtual void frameMove(float dt);    

    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp);

    
    static void setGameLogicServer(GameLogicServerCommon * logic_server); 

    bool collisionCallback(const physics::CollisionInfo & info); 

    void applyImpactImpulse(RigidBody * body,
                            float hit_percentage,
                            const Vector & hit_pos,
                            const Vector & normal,
                            float factor = 1.0f) const;
    
 protected:
    
    Projectile();

    virtual void init();
    
    void handleHit(const ProjectileCollisionInfo & info);

    void handleHit(RigidBody * hit_object, float hit_percentage,
                   const physics::CollisionInfo & info);

    bool splashCollisionCallback(const physics::CollisionInfo & info);
    
    ProjectileCollisionInfo cur_collision_;


    std::string section_;

    unsigned additional_damage_;

    bool water_plane_hit_; ///< After hitting the water plane, don't
                           ///create any more hit feedback...
    
    static std::vector<ProjectileCollisionInfo> splash_hit_; ///< Used for splashCollisionCallback

    static GameLogicServerCommon * game_logic_server_;
};




//------------------------------------------------------------------------------
/**
 *  A ballistic projectile doesn't need to transfer position, it is
 *  sufficient to know starting place & velocity.
 */
class BallisticProjectile : public Projectile
{
 public:

    GAME_OBJECT_IMPL(BallisticProjectile);

    // used for object creation
    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp);

    // used for state value transmission 
    virtual void writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp);

 protected:

    virtual void init();
    
};


#endif
