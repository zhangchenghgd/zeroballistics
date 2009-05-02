
#include "AIPlayerDeathmatch.h"

#include <raknet/RakPeerInterface.h>

#include "ParameterManager.h"
#include "PuppetMasterServer.h"
#include "GameLogicServer.h"
#include "WeaponSystem.h"
#include "Tank.h"

#include "GameLogicServerDeathmatch.h"

#include "Profiler.h"

const float STUCK_THRESHOLD1 = 3.0f;
const float STUCK_THRESHOLD2 = 7.0f;
const float STUCK_THRESHOLD_SUICIDE = 15.0f;

const unsigned HEAL_HP_THRESHOLD = 20;

const float FIRE_ANGLE_THRESHOLD = 0.001f;

const float DELTA_YAW_FACTOR = 14.0f;
const float DELTA_PITCH_FACTOR = 10.0f;
const float DELTA_YAW_FACTOR_THRESHOLD = 0.030f;

//------------------------------------------------------------------------------
AIPlayerDeathmatch::AIPlayerDeathmatch(PuppetMasterServer * puppet_master) :
    AIPlayer(puppet_master),
    enemy_(NULL),
    stuck_dt_(0.0f),
    projectile_inital_velocity_(NULL),
    gravity_(NULL),
    attack_range_sqr_(NULL),
    state_(APS_IDLE)
{
    projectile_inital_velocity_ = s_params.getPointer<float>("splash_cannon.muzzle_velocity"); /// was: tank->getWeaponSystems()[0]->getSection() + ".muzzle_velocity");
    gravity_ = s_params.getPointer<float>("physics.gravity");
    attack_range_sqr_ = s_params.getPointer<float>("server.ai.attack_range_sqr");
}

//------------------------------------------------------------------------------
AIPlayerDeathmatch::~AIPlayerDeathmatch()
{
}

//------------------------------------------------------------------------------
/***
 *  num_ai_players used to get different system addresses for each ai player
 */
void AIPlayerDeathmatch::addPlayer(unsigned num_ai_players)
{
    std::vector<std::string> bot_names = s_params.get<std::vector<std::string> >("server.ai.names");
	std::vector<unsigned> bot_ids = s_params.get<std::vector<unsigned> >("server.ai.ids");

	assert(bot_names.size() == bot_ids.size());

	if(num_ai_players >= bot_names.size())
	{
		s_log << Log::warning << "More bots added than listed in params: server.ai.names, server.ai.ids";
		return;
	}

    std::string name = bot_names[num_ai_players];
	unsigned id_key = bot_ids[num_ai_players];

    pid_ = puppet_master_->getRakPeerInterface()->GetInternalID();
    
    pid_.port += (num_ai_players+1);

    puppet_master_->addPlayer(pid_);    
	puppet_master_->setPlayerData(pid_, name, id_key, id_key);  /// ranking: use id as session key
    puppet_master_->playerReady(pid_);

    assignToTeam();

    sv_player_ = puppet_master_->getPlayer(pid_);
    assert(sv_player_);
    sv_player_->setAIPlayer(this);

}


//------------------------------------------------------------------------------
void AIPlayerDeathmatch::removePlayer()
{
}

//------------------------------------------------------------------------------
void AIPlayerDeathmatch::handleLogic(float dt)
{
    PROFILE(AIPlayerDeathmatch::handleLogic);

    // if player has got no controllable, request respawn and bail
    if(!sv_player_->getControllable())
    {
        state_ = APS_IDLE;

        // if the player has got no controllable delete waypoint
        // positions to goto, avoid going to old WP still in the queue
        ai_target_positions_.clear();

        // check if player has team here (after loadLevel no team possible)
        GameLogicServerCommon * glsc = dynamic_cast<GameLogicServerCommon*>(puppet_master_->getGameLogic());
        if(glsc->getScore().getTeamId(pid_) == INVALID_TEAM_ID)
        {
            assignToTeam();
            return;
        }
    
        // request respawn
        RakNet::BitStream rak_args;
        rak_args.Write(pid_);
        rak_args.Write(0);
        puppet_master_->getGameLogic()->executeCustomCommand(CCCT_REQUEST_RESPAWN, rak_args);
        return;
    }


    Tank * tank = dynamic_cast<Tank*>(sv_player_->getControllable());
    if(tank == NULL) return;

    PlayerInput ai_input; 
    

    switch(state_)
    {
    case APS_IDLE:
        handleFire(dt, ai_input, tank);
        handleWaypoints(dt, ai_input, tank);
        handleStuck(dt, ai_input, tank);
        break;
    case APS_FIRING:
    case APS_DRIVING:
        handleFire(dt, ai_input, tank);
        handleWaypoints(dt, ai_input, tank);
        handleStuck(dt, ai_input, tank);
        handleHeal(dt, ai_input, tank);
        break;
    case APS_HEALING:
        handleHeal(dt, ai_input, tank);
        break;
    default:
        handleFire(dt, ai_input, tank);
        handleWaypoints(dt, ai_input, tank);
        handleStuck(dt, ai_input, tank);
        handleHeal(dt, ai_input, tank);
        break;
    }
 
    // apply processed input
    tank->setPlayerInput(ai_input);

}

//------------------------------------------------------------------------------
void AIPlayerDeathmatch::handleStuck(float dt, PlayerInput & input, Tank * tank)
{
    // track if player got stuck here
    float tank_velocity_sqr = tank->getGlobalLinearVel().lengthSqr();
    if(tank_velocity_sqr < 0.15f)
    {
        stuck_dt_ += dt;
    }else if(tank_velocity_sqr > 3.0f)
    {
        stuck_dt_ = 0.0;
    }


    // if stuck long enough, drive backwards
    if(stuck_dt_ > STUCK_THRESHOLD1 && stuck_dt_ <= STUCK_THRESHOLD2)
    {

        // avoid registering of HitpointTracker::heal all the time here,
        // had serious performance impact
        if(tank->getHitpoints() < tank->getMaxHitpoints())
        {
            state_ = APS_HEALING;
            return;    
        }


         // drive randomly left or right backwards
         bool l_r = (bool)(rand()%2);

		 input.up_ = false;
         input.down_ = true;
         input.left_ = l_r;
         input.right_ = !l_r;
         tank->setPlayerInput(input);
         return;
    } else if(stuck_dt_ > STUCK_THRESHOLD2 && stuck_dt_ <= STUCK_THRESHOLD_SUICIDE)  // if stuck even longer, try jump
    {
         input.action3_ = IKS_DOWN;
         tank->setPlayerInput(input);
         return;
    } else if(stuck_dt_ > STUCK_THRESHOLD_SUICIDE)  // stuck for very long time -> suicide
    {
       GameLogicServerDeathmatch * glsd = dynamic_cast<GameLogicServerDeathmatch*>(puppet_master_->getGameLogic());
       glsd->kill(PKT_SUICIDE, tank);
       stuck_dt_ = 0.0f;
       return;
    }

    return;

}

//------------------------------------------------------------------------------
void AIPlayerDeathmatch::handleFire(float dt, PlayerInput & input, Tank * tank)
{
    if(!enemy_)
    {
		state_ = APS_IDLE;
        getNearestEnemy(tank);
        return;
    }

    // if enemy went outside of fire radius
    // deregister observer and find new enemy
    Vector dist = enemy_->getPosition() - tank->getPosition();
    if(dist.lengthSqr() > (*attack_range_sqr_))
    {
        fp_group_.deregister(ObserverFp(&fp_group_, enemy_, GOE_SCHEDULED_FOR_DELETION));
		state_ = APS_IDLE;
        enemy_ = NULL;
        getNearestEnemy(tank);
        return;
    }

    
    

    /// transform the other tank into the AI players tank space to 
    /// get pos relative to AI tanks transform
    Vector pos_resp_tank =  tank->getTransform().transformPointInv(enemy_->getPosition());
    Vector vel_resp_tank =  tank->getTransform().transformVectorInv(tank->getGlobalLinearVel() - enemy_->getGlobalLinearVel());


    /// calculate the tanks relative velocity into
    /// the positions to get the correct banked yaw
    Vector pos_resp_tank_banking = pos_resp_tank - (vel_resp_tank * 0.4f);


    // take -x,-z to bring angle into 0, 2PI range -> the tanks front
    float target_yaw = atan2(-pos_resp_tank_banking.x_, -pos_resp_tank_banking.z_);
    if (target_yaw < 0) target_yaw += 2 * PI;                

    float current_yaw, current_pitch;
    tank->getTargetTurretPos(current_yaw, current_pitch);

    float new_delta_yaw =  target_yaw - current_yaw;

    /// avoid turning the turret the long way round if yaw in the area of 0.0/2PI
    if(abs(new_delta_yaw) > PI)
    {  
        if(current_yaw > target_yaw)
            new_delta_yaw = target_yaw + (2*PI - current_yaw);
        else
            new_delta_yaw = -(current_yaw + (2*PI - target_yaw));
    }

    // fasten yaw movement
    if(abs(new_delta_yaw) > DELTA_YAW_FACTOR_THRESHOLD) new_delta_yaw *= DELTA_YAW_FACTOR;


    float w = sqrtf(pos_resp_tank.x_ * pos_resp_tank.x_ + pos_resp_tank.z_ * pos_resp_tank.z_);
    float h = pos_resp_tank.y_;

    float alpha = atan2(h,w);

    /// ballistic offset: alpha = 1/2 arcsin(g * x/v0^2)                 
    float alpha_offset = 0.5 * asinf((*gravity_) * w / ( (*projectile_inital_velocity_) * (*projectile_inital_velocity_) ));

    alpha += alpha_offset;

    float new_delta_pitch = alpha - current_pitch;
    
    // fasten pitch movement
    if(abs(new_delta_pitch) > DELTA_YAW_FACTOR_THRESHOLD) new_delta_pitch *= DELTA_PITCH_FACTOR;

    // set new delta yaw / pitch
    input.delta_pitch_ = new_delta_pitch;
    input.delta_yaw_ = new_delta_yaw;

    // only fire if we're aiming close on enemy
    if(new_delta_yaw < FIRE_ANGLE_THRESHOLD &&
       new_delta_pitch < FIRE_ANGLE_THRESHOLD) 
    {
        input.fire1_ = IKS_PRESSED_AND_RELEASED;
    }

	// if we get here, we are in firing state
    state_ = APS_FIRING;

    handleMineLaying(dt, input, tank, target_yaw);

    return;
}

//------------------------------------------------------------------------------
void AIPlayerDeathmatch::handleMineLaying(float dt, PlayerInput & input, Tank * tank, float target_yaw)
{
    const float MINE_LAYING_CONE_ANGLE_RAD = 0.1f; /// angle left+right at tanks backside

    if(target_yaw < (PI - MINE_LAYING_CONE_ANGLE_RAD) ||
       target_yaw > (PI + MINE_LAYING_CONE_ANGLE_RAD)) return;

    /// enemy is behind our tank here, give it 5% chance to lay mine
    if( ((rand()%100)+1) > 95 )
    {
        input.fire3_ = IKS_PRESSED_AND_RELEASED;
    }
    
    return;
}

//------------------------------------------------------------------------------
void AIPlayerDeathmatch::handleWaypoints(float dt, PlayerInput & input, Tank * tank)
{
    //// A-Star path finding here

    // if there are no positions to go to, calc new route
    if(ai_target_positions_.empty())
    {
        WaypointSearchNode start, end;

        s_waypoint_manager_server.getNearestOpenWaypoint(tank->getPosition(), start.x_, start.z_);
        s_waypoint_manager_server.getRandomOpenWaypoint(end.x_, end.z_);       

        ai_target_positions_ = s_waypoint_manager_server.findPath(&start, &end);
    }
    else
    {
        // always travel to the first position in the deque
        Vector new_target_pos = ai_target_positions_.front()->pos_;

        /// get WP pos in tank space
        Vector new_dir = tank->getTransform().transformPointInv(new_target_pos);

        // if point reached 
        if(new_dir.lengthSqr() < 3.0) 
        {
            ai_target_positions_.pop_front();      
            tank->setPlayerInput(input);        
            return;
        }

        float phi = atan2(new_dir.x_, -new_dir.z_);
        if (phi < 0) phi += 2 * PI;  

        phi = rad2Deg(phi);

        
        // set input according
        if(phi >= 350.0 && phi < 361.0) {input.up_ = true; };
        if(phi >= 0.0 && phi < 10.0) {input.up_ = true;};
        if(phi >= 10.0 && phi < 170.0) {input.up_ = true; input.right_ = true;};
        if(phi >= 170.0 && phi < 180.0) {input.down_ = true; input.right_ = true;};
        if(phi >= 180.0 && phi < 190.0) {input.down_ = true; input.left_ = true;};
        if(phi >= 190.0 && phi < 350.0) {input.up_ = true; input.left_ = true;};

		// do not override firing state
		if(state_ != APS_FIRING) state_ = APS_DRIVING;

    }

    return;
}

//------------------------------------------------------------------------------
void AIPlayerDeathmatch::handleHeal(float dt, PlayerInput & input, Tank * tank)
{
    // if tank is not firing and heal below threshold -> stop to heal
    if(tank->getHitpoints() < HEAL_HP_THRESHOLD &&
	   state_ != APS_FIRING &&
       state_ != APS_HEALING)
    {
        input.clear();
        state_ = APS_HEALING;
        return;
    }
    

    if(state_ == APS_HEALING)
    {
        if(tank->getHitpoints() < tank->getMaxHitpoints())
        {
            input.clear();
        }
        else
        {
            state_ = APS_IDLE;
        }
    }

    return;
}

//------------------------------------------------------------------------------
void AIPlayerDeathmatch::assignToTeam() const
{
    RakNet::BitStream rak_args;
    rak_args.Write(pid_);
    rak_args.Write(0);
    puppet_master_->getGameLogic()->executeCustomCommand(CCCT_REQUEST_TEAM_CHANGE, rak_args);
}

//------------------------------------------------------------------------------
/***
  *   Remember nearest enemy, do this to avoid iterating over players 
  *   all the time.
  **/
void AIPlayerDeathmatch::getNearestEnemy(Tank * tank)
{
    // iterate over other tanks
    PuppetMasterServer::PlayerContainer::const_iterator it;
    PuppetMasterServer::PlayerContainer & pc = puppet_master_->getPlayers();

    for(it = pc.begin(); it != pc.end(); it++)
    {
        /// if other player has a controllable
        if(it->getControllable() && 
           (it->getControllable() != sv_player_->getControllable()))
        {
            // if inside fire radius
            Vector dist = it->getControllable()->getPosition() - tank->getPosition();
            if(dist.lengthSqr() < (*attack_range_sqr_))
            {
                enemy_ = it->getControllable();

                enemy_->addObserver(ObserverCallbackFun0(this, &AIPlayerDeathmatch::onEnemyDestroyed),
                                  GOE_SCHEDULED_FOR_DELETION,
                                  &fp_group_);
                return;
            }
        }
    }

    /// no enemy found nearby
    enemy_ = NULL;

}

//------------------------------------------------------------------------------
void AIPlayerDeathmatch::onEnemyDestroyed()
{
    enemy_ = NULL;
}
