#include "Projectile.h"

#include <raknet/BitStream.h>

#include "physics/OdeRigidBody.h"
#include "physics/OdeSimulator.h"

#include "GameLogicServerCommon.h"
#include "AutoRegister.h"
#include "Tank.h"
#include "PuppetMasterServer.h"
#include "GameState.h"
#include "ParameterManager.h"
#include "Water.h"
#include "NetworkCommand.h"

#undef min
#undef max

GameLogicServerCommon * Projectile::game_logic_server_ = NULL;


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
const std::string & Projectile::getSection() const
{
    return section_;
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
    
    network::writeToBitstream(stream, section_); /// needed for client side impact only ATM!
}

//------------------------------------------------------------------------------
void Projectile::readInitValuesFromBitstream(RakNet::BitStream & stream,
                                             GameState * game_state,
                                             uint32_t timestamp)
{
    RigidBody::readInitValuesFromBitstream(stream,
                                           game_state,
                                           timestamp);

    
    network::readFromBitstream(stream, section_); /// needed for client side impact only ATM!
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
    
    // Ignore water plane on server so we can still shoot e.g. mines
    // below water
    if (other_body && other_body->getType() == "Water")
    {
        water_plane_hit_ = true;
        game_logic_server_->onProjectileHit(this, other_body, 1.0f, info, true);
        return false;
    }

    // Ignore collision with own tank
    if (other_body &&
        projectile->getOwner() == other_body->getOwner()) return false;
    
    // Update only if hit is closer than previous one
    if (info.penetration_ < cur_collision_.info_.penetration_ || cur_collision_.game_object_id_ == INVALID_GAMEOBJECT_ID)
    {        
        cur_collision_.info_ = info;
        if (other_body)
        {
            cur_collision_.game_object_id_ = other_body->getId();
        } else cur_collision_.game_object_id_ = INVALID_GAMEOBJECT_ID;
    }
   
    return false;
}


//------------------------------------------------------------------------------
/**
 *  \param body The hit body.
 *
 *  \param hit_percentage 1.0 for direct hit, smaller value for splash
 *  attenuation.
 *
 *  \param hit_pos The projectile impact spot. Either on the hit body
 *  for direct hit, or somewhere else for splash hit.
 *
 *  \param normal Only used if hit_percentage is 1.0 (direct hit, not
 *  splash).
 */
void Projectile::applyImpactImpulse(RigidBody * body,
                                    float hit_percentage,
                                    const Vector & hit_pos,
                                    const Vector & normal,
                                    float factor) const
{
    if (!body || body->isStatic()) return;

    body->setSleeping(false);


    // assume hit body is at rest, projectile moves together with hit
    // body after collision
    //
    // =>  mp*vp = (mp+m)*v'
    //     v' = vp * mp / (mp+m)
    //     I = m*v'
    float proj_mass = s_params.get<float>(section_ + ".mass");
    float f = (body->getTarget()->getMass() * proj_mass /
               (body->getTarget()->getMass() + proj_mass));

    f *= factor;
        
    if (hit_percentage == 1.0f)
    {
        Vector proj_dir = getGlobalLinearVel();
        float speed = proj_dir.length();
        proj_dir /= speed;
        
//         float cos_alpha_sqr = vecDot(&proj_dir, &normal);
//         cos_alpha_sqr *= cos_alpha_sqr;

        Vector impulse_dir = proj_dir - /*(1.0f - cos_alpha_sqr) * */ normal;
        impulse_dir.normalize();
        
        body->getTarget()->addGlobalForceAtGlobalPos(impulse_dir * speed * f * s_params.get<float>("physics.fps"),
                                                     hit_pos);
    } else
    {
        Vector impulse = body->getPosition() - hit_pos;
        impulse.normalize();
        impulse *= s_params.get<float>(section_ + ".splash_impulse") * f * hit_percentage;

        body->getTarget()->addGlobalForce(impulse * s_params.get<float>("physics.fps"));
    }
}


//------------------------------------------------------------------------------
Projectile::Projectile() :
    additional_damage_(0),
    water_plane_hit_(false)
{
}


//------------------------------------------------------------------------------
void Projectile::init()
{
    setSleeping(false);    
}


//------------------------------------------------------------------------------
void Projectile::handleHit(const ProjectileCollisionInfo & cur_info)
{
    RigidBody * hit_object = (RigidBody*)
        game_logic_server_->getPuppetMaster()->getGameState()->getGameObject(cur_info.game_object_id_);


    // First, determine objects hit by splash effect if applicable. Do
    // this first, or objects which are created by replacing directly
    // hit object may be affected.
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
    }    

    // Handle the direct hit which caused this call.
    game_logic_server_->onProjectileHit(this, hit_object, 1.0f, cur_info.info_, !water_plane_hit_);
    
    // now handle splash hits.
    for (unsigned i=0; i<splash_hit_.size(); ++i)
    {
        RigidBody * splash_hit_object = (RigidBody*)
            game_logic_server_->getPuppetMaster()->getGameState()->getGameObject(splash_hit_[i].game_object_id_);
            
        // Dont' apply splash damage to directly hit object
        if (splash_hit_object == hit_object) continue;

        // make sure distance never exceeds actual splash radius
        float percentage = std::min(splash_hit_[i].info_.penetration_ / splash_radius, 1.0f);

        game_logic_server_->onProjectileHit(this, splash_hit_object, percentage, cur_info.info_, !water_plane_hit_);
    }
    splash_hit_.clear();
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









REGISTER_CLASS(GameObject, BallisticProjectile);


//------------------------------------------------------------------------------
void BallisticProjectile::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    Projectile::writeInitValuesToBitstream(stream);
}

//------------------------------------------------------------------------------
void BallisticProjectile::readInitValuesFromBitstream(RakNet::BitStream & stream,
                                                      GameState * game_state,
                                                      uint32_t timestamp)
{
    // don't perform dead reckoning for initial position, let proxy be
    // created there...
    Projectile::readInitValuesFromBitstream(stream, game_state, 0);
}

//------------------------------------------------------------------------------
/**
 *  We just want to send initial projectile state, projectile is never
 *  updated from server. Empty packests are not sent at all (see
 *  NetworkCommandServer::send())
 */
void BallisticProjectile::writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const
{
    if (type == OST_BOTH) Projectile::writeStateToBitstream(stream, type);
}

//------------------------------------------------------------------------------
void BallisticProjectile::readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp)
{
    assert(type == OST_BOTH);

    readCoreState(stream, 0, true);    
    
    // Initialize continuous collision detection for the projectile's
    // proxy with the non-dead-reckoned position
    //
    // This is important because we might shoot through stuff
    // otherwise.
    assert(proxy_object_);
    assert(proxy_object_->getGeoms().size() == 1);
    warpProxy(true);
    physics::OdeContinuousGeom * cont_geom =
        dynamic_cast<physics::OdeContinuousGeom*>(proxy_object_->getGeoms()[0]);
    assert(cont_geom);
    cont_geom->frameMove();

    // Now perform dead reckoning (with gravity)
    Matrix m = target_object_->getTransform();
    Vector v = target_object_->getGlobalLinearVel();
    Vector w = target_object_->getGlobalAngularVel();

    deadReckon(m, v, w, true, timestamp);
    
    target_object_->setTransform(m);
    target_object_->setGlobalLinearVel(v);    

    // warp to dead reckoned position
    warpProxy(true);
}

//------------------------------------------------------------------------------
/**
 *  We won't receive server corrections, so apply gravity to target.
 */
void BallisticProjectile::init()
{
    Projectile::init();
    target_object_->enableGravity(true);
}
