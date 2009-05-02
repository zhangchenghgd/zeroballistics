
#include "TankMine.h"

#include <limits>

#include <raknet/BitStream.h>

#include "Tank.h"
#include "AutoRegister.h"
#include "ParameterManager.h"

#include "GameLogicServerCommon.h" 

#undef min
#undef max

GameLogicServerCommon * TankMine::game_logic_server_ = NULL;

REGISTER_CLASS(GameObject, TankMine);


//------------------------------------------------------------------------------
TankMine::~TankMine()
{
}

//------------------------------------------------------------------------------
void TankMine::frameMove(float dt)
{
    RigidBody::frameMove(dt);

    if(getProxy()) return; // handle only on server side

    if(lifetime_ <= 0.0f)
    {
        scheduleForDeletion();
    }
    else
    {
        lifetime_ -= dt;
    }
    
}

//------------------------------------------------------------------------------
void TankMine::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    RigidBody::writeInitValuesToBitstream(stream);

    stream.Write(team_id_);
}

//------------------------------------------------------------------------------
void TankMine::readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp)
{
    RigidBody::readInitValuesFromBitstream(stream, game_state, timestamp);

    TEAM_ID team_id;
    stream.Read(team_id);

    setTeamId(team_id);
}

//------------------------------------------------------------------------------
void TankMine::setGameLogicServer(GameLogicServerCommon * logic)
{
    game_logic_server_ = logic;
}

//------------------------------------------------------------------------------
bool TankMine::collisionCallback(const physics::CollisionInfo & info)
{
    // early bail out on coll. with static
    if(info.type_ != physics::CT_START || info.other_geom_->isStatic()) return true;

    assert((TankMine*)info.this_geom_ ->getUserData() == this);
    
    RigidBody * other = (RigidBody*)info.other_geom_->getUserData();
    
    if(other->getType() == "Tank")
    {
        Tank * tank = (Tank*)other;

        unsigned damage = s_params.get<unsigned>(section_ + ".damage");

        // we can hit ourselves with splash damage, damage to self is
        // always afflicted by UNASSIGNED_SYSTEM_ADDRESS
        SystemAddress damage_dealer = getOwner() == tank->getOwner() ? UNASSIGNED_SYSTEM_ADDRESS : getOwner();
        
        tank->dealWeaponDamage(damage, damage_dealer);

        // apply impact impulse when running over a mine
        Vector impulse = this->getTransform().getY();
        impulse.normalize();
        impulse *= s_params.get<float>(section_ + ".impact_impulse");
        tank->getTarget()->addGlobalForceAtGlobalPos(impulse * s_params.get<float>("physics.fps"),
                                                               info.pos_);

        if (tank->getHitpoints() <= 0)
        {
            game_logic_server_->kill(PKT_MINE, tank, damage_dealer);
        }

        explode(tank->getOwner(), WHT_MINE_HIT_TANK);
    } else explode(UNASSIGNED_SYSTEM_ADDRESS, WHT_MINE_HIT_OTHER);
    
    return true;
}


//------------------------------------------------------------------------------
void TankMine::setSection(const std::string & section)
{
    section_ = section;
}

//------------------------------------------------------------------------------
void TankMine::setTeamId(TEAM_ID tid)
{
    team_id_ = tid;
}

//------------------------------------------------------------------------------
TEAM_ID TankMine::getTeamId() const
{
    return team_id_;
}

//------------------------------------------------------------------------------
void TankMine::setLifetime(float lifetime)
{
    lifetime_ = lifetime;
}

//------------------------------------------------------------------------------
void TankMine::explode(const SystemAddress & hit_player,
                       WEAPON_HIT_TYPE feedback_type)
{
    physics::CollisionInfo info;

    info.pos_ = getPosition();
    info.n_ = Vector(0,1,0);
    
    game_logic_server_->sendWeaponHit(getOwner(), hit_player, info, feedback_type);
    scheduleForDeletion();
}




//------------------------------------------------------------------------------
TankMine::TankMine() :
    lifetime_(0.0f),
    team_id_(INVALID_TEAM_ID)
{
}
