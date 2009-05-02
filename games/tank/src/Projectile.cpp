#include "Projectile.h"

#include <raknet/BitStream.h>

#include "physics/OdeRigidBody.h"

#include "GameLogicServerCommon.h"
#include "AutoRegister.h"
#include "Tank.h"
#include "PuppetMasterServer.h"
#include "GameState.h"
#include "ParameterManager.h"
#include "physics/OdeSimulator.h"

#undef min
#undef max

GameLogicServerCommon * Projectile::game_logic_server_ = NULL;

REGISTER_CLASS(GameObject, Projectile);

std::vector<ProjectileCollisionInfo> Projectile::splash_hit_;


//------------------------------------------------------------------------------
Projectile::~Projectile()
{
}


//------------------------------------------------------------------------------
void Projectile::setSection(const std::string & s)
{
    section_ = s;
}

//------------------------------------------------------------------------------
void Projectile::setAdditionalDamage(unsigned dam)
{
    additional_damage_ = dam;
}

//------------------------------------------------------------------------------
float Projectile::getSplashRadius() const
{
    return s_params.get<float>(section_ + ".splash_radius");
}

//------------------------------------------------------------------------------
float Projectile::getDamage() const
{
    return additional_damage_ + s_params.get<unsigned>(section_ + ".damage");
}


//------------------------------------------------------------------------------
float Projectile::getImpactImpulse() const
{
    return s_params.get<float>(section_ + ".impact_impulse");
}

//------------------------------------------------------------------------------
void Projectile::frameMove(float dt)
{
    if(cur_collision_.info_.penetration_ != 0.0f)
    {
        handleHit(cur_collision_);
        scheduleForDeletion();
    }

    RigidBody::frameMove(dt);
}

//------------------------------------------------------------------------------
void Projectile::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    RigidBody::writeInitValuesToBitstream(stream);
}

//------------------------------------------------------------------------------
void Projectile::readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state)
{
    RigidBody::readInitValuesFromBitstream(stream, game_state);
}

//------------------------------------------------------------------------------
/**
 *  We just want to send initial projectile state, projectile is never
 *  updated from server. Empty packests are not sent at all (see
 *  NetworkCommandServer::send())
 */
void Projectile::writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const
{
    if (type == OST_BOTH) RigidBody::writeStateToBitstream(stream, type);
}

//------------------------------------------------------------------------------
/**
 *  Because sound effects etc. are triggered by client side collision,
 *  we want to make sure the projectile collides before it is deleted
 *  by the server. Thus we move it forward some amount. This works
 *  only because the projectile isn't updated by the server.
 */
void Projectile::readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp)
{
    assert(type == OST_BOTH);

    readCoreState(stream, 0);    
    warpProxy(true);
    
    // Initialize continuous collision detection for the projectile's
    // proxy with the non-dead-reckoned position
    assert(proxy_object_);
    assert(proxy_object_->getGeoms().size() == 1);
    physics::OdeContinuousGeom * cont_geom =
        dynamic_cast<physics::OdeContinuousGeom*>(proxy_object_->getGeoms()[0]);
    assert(cont_geom);
    cont_geom->frameMove();

    // Now perform dead reckoning
    Matrix m = target_object_->getTransform();
    Vector v = target_object_->getGlobalLinearVel();
    Vector w = target_object_->getGlobalAngularVel();

    deadReckon(m, v, w, true, timestamp);
    
    target_object_->setTransform(m);
    target_object_->setGlobalLinearVel(v);    
}

//------------------------------------------------------------------------------
void Projectile::setGameLogicServer(GameLogicServerCommon * logic)
{
    game_logic_server_ = logic;
}

//------------------------------------------------------------------------------
/**
 *  Server side function. Updates cur_collision_ to reflect the
 *  nearest hit. This is neccessary because multiple objects can be
 *  hit at the same time by the ray used for continuous collision
 *  detection.
 */
bool Projectile::collisionCallback(const physics::CollisionInfo & info)
{
    assert(game_logic_server_);
    
    if (info.type_ != physics::CT_START) return false;

    Projectile * projectile = (Projectile*)info.this_geom_ ->getUserData();
    RigidBody  * other_body = (RigidBody*) info.other_geom_->getUserData();

    // Ignore collision with own tank
    if (other_body &&
        projectile->getOwner() == other_body->getOwner()) return false;
    
    // Update only if hit is closer than previous one
    if (info.penetration_ < cur_collision_.info_.penetration_ || cur_collision_.game_object_id_ == INVALID_GAMEOBJECT_ID)
    {
        GameObject * go = (GameObject*)info.other_geom_->getUserData();
        
        cur_collision_.info_     = info;
        if (go) cur_collision_.game_object_id_ = go->getId();        
    }
   
    return false;
}

//------------------------------------------------------------------------------
Projectile::Projectile() :
    additional_damage_(0)
{
}

//------------------------------------------------------------------------------
/**
 *  We won't receive server corrections, so apply gravity to target.
 */
void Projectile::init()
{
    target_object_->enableGravity(true);
}

//------------------------------------------------------------------------------
void Projectile::handleHit(const ProjectileCollisionInfo & cur_info)
{
    RigidBody * hit_object = (RigidBody*)
        game_logic_server_->getPuppetMaster()->getGameState()->getGameObject(cur_info.game_object_id_);

    // First, handle the direct hit which caused this call.
    game_logic_server_->onProjectileHit(this, hit_object, 1.0f, cur_info.info_);

    float splash_radius = getSplashRadius();

    if (splash_radius != 0.0f)
    {
        assert(splash_radius > 0.0f);
        // Create a sphere with the specified radius and collide it
        // against the world space
        physics::OdeSimulator * sim =
            game_logic_server_->getPuppetMaster()->getGameState()->getSimulator();

        physics::OdeSphereGeom sphere(splash_radius);
        sphere.setPosition(cur_info.info_.pos_);
        sphere.setCategory(CCCS_PROJECTILE, sim);

        sim->getStaticSpace()->collide(&sphere,
                                       physics::CollisionCallback(this, &Projectile::splashCollisionCallback));
        sim->getActorSpace()->collide(&sphere,
                                      physics::CollisionCallback(this, &Projectile::splashCollisionCallback));

        for (unsigned i=0; i<splash_hit_.size(); ++i)
        {
            RigidBody * splash_hit_object = (RigidBody*)
                game_logic_server_->getPuppetMaster()->getGameState()->getGameObject(splash_hit_[i].game_object_id_);
            
            // Dont' apply splash damage to directly hit object
            if (splash_hit_object == hit_object) continue;

            // make sure distance never exceeds actual splash radius
            float percentage = std::min(splash_hit_[i].info_.penetration_ / splash_radius, 1.0f);

            game_logic_server_->onProjectileHit(this, splash_hit_object, percentage, cur_info.info_);
        }

        splash_hit_.clear();
    }
}

//------------------------------------------------------------------------------
bool Projectile::splashCollisionCallback(const physics::CollisionInfo & info)
{
    assert(info.type_ == physics::CT_SINGLE);

    RigidBody * body = (RigidBody*)info.other_geom_->getUserData();

    for (unsigned i=0; i<splash_hit_.size(); ++i)
    {
        // Don't count the same body twice (can have more than one
        // geom)
        if (splash_hit_[i].game_object_id_ == (body ? body->getId() : INVALID_GAMEOBJECT_ID))
        {
            if (info.penetration_ > splash_hit_[i].info_.penetration_)
            {
                splash_hit_[i].info_ = info;
            }
            
            return false;
        }
    }
    
    splash_hit_.push_back(ProjectileCollisionInfo());
    splash_hit_.back().game_object_id_ = body ? body->getId() : INVALID_GAMEOBJECT_ID;
    splash_hit_.back().info_ = info;

    return false;
}
