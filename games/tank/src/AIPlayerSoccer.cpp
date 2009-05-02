
#include "AIPlayerSoccer.h"

#include <raknet/RakPeerInterface.h>

#include "ParameterManager.h"
#include "PuppetMasterServer.h"
#include "GameLogicServer.h"
#include "WeaponSystem.h"
#include "Tank.h"

#include "GameLogicServerSoccer.h"
#include "SoccerBall.h"

#include "Profiler.h"

const float STUCK_THRESHOLD1 = 3.0f;
const float STUCK_THRESHOLD2 = 7.0f;
const float STUCK_THRESHOLD_SUICIDE = 15.0f;


const float DELTA_YAW_FACTOR = 14.0f;
const float DELTA_PITCH_FACTOR = 10.0f;
const float DELTA_YAW_FACTOR_THRESHOLD = 0.030f;

const float BALL_INSIDE_DRAG_RANGE = 4.0f;
const float GOAL_INSIDE_SHOOT_RANGE = 10.0f;

const float ENEMY_INSIDE_RAM_RANGE_MAX = 12.0f;
const float ENEMY_INSIDE_RAM_RANGE_MIN = 5.0f;

const float BALL_MOVEMENT_RANGE_SQR = 10.0f;

//------------------------------------------------------------------------------
AIPlayerSoccer::AIPlayerSoccer(PuppetMasterServer * puppet_master) :
    AIPlayer(puppet_master),
    enemy_(NULL),
    stuck_dt_(0.0f),
    projectile_inital_velocity_(NULL),
    gravity_(NULL),
    attack_range_sqr_(NULL),
    state_(APSS_IDLE)
{
	old_ball_pos_ = Vector(0.0,0.0,0.0);

    projectile_inital_velocity_ = s_params.getPointer<float>("splash_cannon.muzzle_velocity"); /// was: tank->getWeaponSystems()[0]->getSection() + ".muzzle_velocity");
    gravity_ = s_params.getPointer<float>("physics.gravity");
    attack_range_sqr_ = s_params.getPointer<float>("server.ai.attack_range_sqr");
}

//------------------------------------------------------------------------------
AIPlayerSoccer::~AIPlayerSoccer()
{
}

//------------------------------------------------------------------------------
/***
 *  num_ai_players used to get different system addresses for each ai player
 */
void AIPlayerSoccer::addPlayer(unsigned num_ai_players)
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
void AIPlayerSoccer::removePlayer()
{
}

//------------------------------------------------------------------------------
void AIPlayerSoccer::handleLogic(float dt)
{
    PROFILE(AIPlayerSoccer::handleLogic);

    // if player has got no controllable, request respawn and bail
    if(!sv_player_->getControllable())
    {
        state_ = APSS_IDLE;

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
    

	/*
		AI Soccer player should do:
			- get near ball
				- if ball in front of tank -> tractor
				- if not -> shoot at ball
			- if ball between claws -> use beam and move to opponents goal fast


	*/

	// TODO better ram enemy call


    switch(state_)
    {
    case APSS_IDLE:
	case APSS_GO_NEAR_SOCCERBALL:
	case APSS_GO_TO_OWN_SPAWNPOS:
        handleIdle(dt, ai_input, tank);
        handleWaypoints(dt, ai_input, tank);
		handleHitEnemy(dt, ai_input, tank);
        handleStuck(dt, ai_input, tank);
        break;
	case APSS_DRAG_BALL_TOWARDS_TANK:
		handleDragBall(dt, ai_input, tank);
        handleStuck(dt, ai_input, tank);
		break;
	case APSS_GO_TO_ENEMY_GOAL:
		handleDragBall(dt, ai_input, tank);
        handleWaypoints(dt, ai_input, tank);
        handleStuck(dt, ai_input, tank);
		break;
	case APSS_RAM_ENEMY:
		handleRamEnemy(dt, ai_input, tank);
        handleStuck(dt, ai_input, tank);
		break;
    }
 
    // apply processed input
    tank->setPlayerInput(ai_input);

}

//------------------------------------------------------------------------------
void AIPlayerSoccer::handleIdle(float dt, PlayerInput & input, Tank * tank)
{
	GameLogicServerSoccer * glss = static_cast<GameLogicServerSoccer*>(puppet_master_->getGameLogic());	

	if(glss->soccer_ball_)
	{
		state_ = APSS_GO_NEAR_SOCCERBALL;


		// check if balls near enough to go into "drag  near tank" state

		/// transform the soccer ball into the AI players tank space to 
		/// get pos relative to AI tanks transform
		Vector pos_resp_tank =  tank->getTransform().transformPointInv(glss->soccer_ball_->getPosition());

		if(pos_resp_tank.length() < BALL_INSIDE_DRAG_RANGE)
		{
			state_ = APSS_DRAG_BALL_TOWARDS_TANK;
			return;
		}


	}
	else
	{
		state_ = APSS_GO_TO_OWN_SPAWNPOS;
		return;
	}


	

}

//------------------------------------------------------------------------------
void AIPlayerSoccer::handleStuck(float dt, PlayerInput & input, Tank * tank)
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
       GameLogicServerSoccer * glss = dynamic_cast<GameLogicServerSoccer*>(puppet_master_->getGameLogic());
       glss->kill(PKT_SUICIDE, tank);
       stuck_dt_ = 0.0f;
       return;
    }

    return;

}

//------------------------------------------------------------------------------
void AIPlayerSoccer::handleWaypoints(float dt, PlayerInput & input, Tank * tank)
{
	GameLogicServerSoccer * glss = static_cast<GameLogicServerSoccer*>(puppet_master_->getGameLogic());	

    //// A-Star path finding here

    // if there are no positions to go to, calc new route
    if(ai_target_positions_.empty())
    {
        WaypointSearchNode start, end;
		s_waypoint_manager_server.getNearestOpenWaypoint(tank->getPosition(), start.x_, start.z_);
		
		if(state_ == APSS_GO_NEAR_SOCCERBALL)
		{			
			assert(glss->soccer_ball_);
			old_ball_pos_ = glss->soccer_ball_->getPosition();
			s_waypoint_manager_server.getNearestOpenWaypoint(old_ball_pos_, end.x_, end.z_);       
		} else if(state_ == APSS_GO_TO_OWN_SPAWNPOS)
		{
			Vector start_pos = glss->getRespawnPos(pid_, 0).getTranslation();
			s_waypoint_manager_server.getNearestOpenWaypoint(start_pos, end.x_, end.z_);       
		} else if(state_ == APSS_GO_TO_ENEMY_GOAL)
		{
			TEAM_ID my_team = glss->getScore().getTeamId(pid_);
			Vector enemy_goal_pos = glss->goal_pos_[1-my_team];
			s_waypoint_manager_server.getNearestOpenWaypoint(enemy_goal_pos, end.x_, end.z_);       
		}

        ai_target_positions_ = s_waypoint_manager_server.findPath(&start, &end);
    }
    else
    {
		// special check for APSS_GO_NEAR_SOCCERBALL state, if the ball has moved
		// to far away -> calculate new route
		if(state_ == APSS_GO_NEAR_SOCCERBALL && glss->soccer_ball_)
		{
			Vector curr_ball_pos = glss->soccer_ball_->getPosition();
			float ball_moved_distance_sqr = (old_ball_pos_ - curr_ball_pos).lengthSqr();
			

			// if ball moved to far, clear deque and find new path to ball
			if(ball_moved_distance_sqr > BALL_MOVEMENT_RANGE_SQR)
			{
				ai_target_positions_.clear();
				return;
			}
		}


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


		// slow down tank if it's to fast to get the narrow curves
		bool acceleration = true;
		if(tank->getGlobalLinearVel().length() > 2.5f)
		{
			acceleration = false;
		}

        // set input according
        if(phi >= 350.0 && phi < 361.0) {input.up_ = true; };
        if(phi >= 0.0 && phi < 10.0) {input.up_ = true;};
        if(phi >= 10.0 && phi < 170.0) {input.up_ = acceleration; input.right_ = true;};
        if(phi >= 170.0 && phi < 180.0) {input.down_ = true; input.right_ = true;};
        if(phi >= 180.0 && phi < 190.0) {input.down_ = true; input.left_ = true;};
        if(phi >= 190.0 && phi < 350.0) {input.up_ = acceleration; input.left_ = true;};
    }

}

//------------------------------------------------------------------------------
void AIPlayerSoccer::handleDragBall(float dt, PlayerInput & input, Tank * tank)
{
	GameLogicServerSoccer * glss = static_cast<GameLogicServerSoccer*>(puppet_master_->getGameLogic());	

	// at first check if drag ball to tank condition is still true
	if(glss->soccer_ball_)
	{
		float distance_to_ball = (tank->getPosition() - glss->soccer_ball_->getPosition()).length();

		if(distance_to_ball > (1.5*BALL_INSIDE_DRAG_RANGE))
		{
			state_ = APSS_IDLE;
			ai_target_positions_.clear(); // make the bot find new ways
			return;
		}
	}
	else
	{
		state_ = APSS_IDLE;
		ai_target_positions_.clear(); // make the bot find new ways
		return;
	}

	


    /// transform the ball into the AI players tank space to 
    /// get pos relative to AI tanks transform
    Vector pos_resp_tank = tank->getTransform().transformPointInv(glss->soccer_ball_->getPosition());


    // take -x,-z to bring angle into 0, 2PI range -> the tanks front
    float target_yaw = atan2(-pos_resp_tank.x_, -pos_resp_tank.z_);
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


	// calc pitch
    float w = sqrtf(pos_resp_tank.x_ * pos_resp_tank.x_ + pos_resp_tank.z_ * pos_resp_tank.z_);
    float h = pos_resp_tank.y_;

    float alpha = -atan2(h,w);
    float new_delta_pitch = alpha - current_pitch;
    
    // fasten pitch movement
    if(abs(new_delta_pitch) > DELTA_YAW_FACTOR_THRESHOLD) new_delta_pitch *= DELTA_PITCH_FACTOR;

    // set new delta yaw / pitch
    input.delta_pitch_ = new_delta_pitch;
    input.delta_yaw_ = new_delta_yaw;

	input.fire2_ = IKS_DOWN;


    // if we're aiming close on ball and it is near, set move to goal state
	if( ((target_yaw < 0.2f) || target_yaw > (2*PI - 0.2f))  && w < 2.5f)
    {
		state_ = APSS_GO_TO_ENEMY_GOAL;

		// if we are here: the ball is in front of us and we are moving towards
		// the enemies goal -> check if the goal is in front of us and if it's
		// near try to shoot the ball into the goal
		TEAM_ID my_team = glss->getScore().getTeamId(pid_);
		Vector enemy_goal_pos = glss->goal_pos_[1-my_team];

		Vector goal_pos_resp_tank = tank->getTransform().transformPointInv(enemy_goal_pos);

		// take -x,-z to bring angle into 0, 2PI range -> the tanks front
		float goal_yaw = atan2(-goal_pos_resp_tank.x_, -goal_pos_resp_tank.z_);
		if (goal_yaw < 0) goal_yaw += 2 * PI;

		float goal_phi = rad2Deg(goal_yaw);

        // set input according
        if( (goal_phi >= 350.0 && goal_phi < 361.0) ||
			(goal_phi >= 0.0 && goal_phi < 10.0) )
		{
			// if tank is in range
			if(goal_pos_resp_tank.length() < GOAL_INSIDE_SHOOT_RANGE)
			{
				input.fire2_ = IKS_UP; // stop the tactor beam
				input.fire1_ = IKS_PRESSED_AND_RELEASED; // and shoot
			}
		}
    }
	else
	{
		state_ = APSS_DRAG_BALL_TOWARDS_TANK;

		// it can happen that we're dragging the ball always near our chassis
		// but not our claws, give it a  3% chance to fire the ball away then
		// to avoid locks
		if( ((rand()%100)+1) > 97 )
		{
			input.fire1_ = IKS_PRESSED_AND_RELEASED;
		}
	}


}

//------------------------------------------------------------------------------
void AIPlayerSoccer::handleHitEnemy(float dt, PlayerInput & input, Tank * tank)
{

		// early bail if no enemy set
		if(enemy_ == NULL)
		{
			getNearestEnemy(tank);
			return;
		}		

		/// transform the enemy into the AI players tank space to 
		/// get pos relative to AI tanks transform
		Vector pos_resp_tank = tank->getTransform().transformPointInv(enemy_->getPosition());

		// take -x,-z to bring angle into 0, 2PI range -> the tanks front
		float target_yaw = atan2(-pos_resp_tank.x_, -pos_resp_tank.z_);
		if (target_yaw < 0) target_yaw += 2 * PI;

		float phi = rad2Deg(target_yaw);

        // set input according
        if( (phi >= 340.0 && phi < 361.0) ||
			(phi >= 0.0 && phi < 20.0) )
		{
			// if tank is in range
			float distance = pos_resp_tank.length();
			if(distance < ENEMY_INSIDE_RAM_RANGE_MAX &&
			   distance > ENEMY_INSIDE_RAM_RANGE_MIN)
			{
				// give it a 20% chance to go into the ram state
				if( ((rand()%100)+1) > 80 )
				{
					state_ = APSS_RAM_ENEMY;
				}				
			}
		}
		
}

//------------------------------------------------------------------------------
void AIPlayerSoccer::handleRamEnemy(float dt, PlayerInput & input, Tank * tank)
{
		// early bail if enemy is gone
		if(enemy_ == NULL)
		{
			state_ = APSS_IDLE;
			getNearestEnemy(tank);
			return;
		}

		/// transform the enemy into the AI players tank space to 
		/// get pos relative to AI tanks transform
		Vector pos_resp_tank = tank->getTransform().transformPointInv(enemy_->getPosition());

		// take -x,-z to bring angle into 0, 2PI range -> the tanks front
		float target_yaw = atan2(-pos_resp_tank.x_, -pos_resp_tank.z_);
		if (target_yaw < 0) target_yaw += 2 * PI;

		float phi = rad2Deg(target_yaw);

		// also shoot in front
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
	
		// set new delta yaw / pitch
		input.delta_yaw_ = new_delta_yaw;

        // set input according
        if( (phi >= 340.0 && phi < 361.0) ||
			(phi >= 0.0 && phi < 20.0) )
		{
			// if tank is in range -> ram
			float distance = pos_resp_tank.length();
			if(distance < ENEMY_INSIDE_RAM_RANGE_MAX)
			{
				input.fire1_ = IKS_PRESSED_AND_RELEASED; // fire
				input.up_ = true;
				input.fire4_ = IKS_DOWN;	// boost
				return;
			}
		}

		// if we get here the ram state is not valid anymore
		state_ = APSS_IDLE;
}

//------------------------------------------------------------------------------
void AIPlayerSoccer::assignToTeam() const
{
    GameLogicServerCommon * glsc = dynamic_cast<GameLogicServerCommon*>(puppet_master_->getGameLogic());
	TEAM_ID smallest_team = glsc->getScore().getSmallestTeam();

    RakNet::BitStream rak_args;
    rak_args.Write(pid_);
    rak_args.Write(smallest_team);
    puppet_master_->getGameLogic()->executeCustomCommand(CCCT_REQUEST_TEAM_CHANGE, rak_args);
}

//------------------------------------------------------------------------------
/***
  *   Remember nearest enemy, do this to avoid iterating over players 
  *   all the time.
  **/
void AIPlayerSoccer::getNearestEnemy(Tank * tank)
{
    // iterate over other tanks
    PuppetMasterServer::PlayerContainer::const_iterator it;
    PuppetMasterServer::PlayerContainer & pc = puppet_master_->getPlayers();

	// used to check for opponents team
    GameLogicServerCommon * glsc = dynamic_cast<GameLogicServerCommon*>(puppet_master_->getGameLogic());

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
				// check for opponents team
				if(glsc->getScore().getTeamId(pid_) != glsc->getScore().getTeamId(it->getId()))
				{
					enemy_ = it->getControllable();

					enemy_->addObserver(ObserverCallbackFun0(this, &AIPlayerSoccer::onEnemyDestroyed),
									  GOE_SCHEDULED_FOR_DELETION,
									  &fp_group_);
					return;
				}
            }
        }
    }

    /// no enemy found nearby
    enemy_ = NULL;

}

//------------------------------------------------------------------------------
void AIPlayerSoccer::onEnemyDestroyed()
{
    enemy_ = NULL;
}
