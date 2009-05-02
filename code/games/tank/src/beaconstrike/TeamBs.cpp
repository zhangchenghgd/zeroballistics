
#include "TeamBs.h"


#include "physics/OdeCollision.h"
#include "physics/OdeRigidBody.h"


#include "Log.h"
#include "BeaconBoundaryServer.h"
#include "ParameterManager.h"

#ifndef DEDICATED_SERVER
#include "BeaconBoundaryClient.h"
#endif

//------------------------------------------------------------------------------
ServerTeam::ServerTeam()
{
    reset();

    s_console.addVariable("beacon_queue_size", &beacon_queue_, &fp_group_);
}


//------------------------------------------------------------------------------
ServerTeam::~ServerTeam()
{
}

//------------------------------------------------------------------------------
void ServerTeam::reset()
{
    beacon_queue_ = s_params.get<float>("server.logic_beaconstrike.num_initial_beacons");
}

//------------------------------------------------------------------------------
std::string ServerTeam::getTankName() const
{
    return s_params.get<std::string>(config_name_ + ".tank_name");
}

//------------------------------------------------------------------------------
std::string ServerTeam::getBeaconName() const
{
    return s_params.get<std::string>(config_name_ + ".beacon_name");
}


//------------------------------------------------------------------------------
bool ServerTeam::getBeaconFromQueue()
{
    if (beacon_queue_ >= 1.0f)
    {
        beacon_queue_ -= 1.0f;
        return true;
    } else return false;
}

//------------------------------------------------------------------------------
void ServerTeam::fillBeaconQueue(float amount)
{
    beacon_queue_ += amount;
}

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
ClientTeam::ClientTeam() :
    beacon_boundary_(NULL)
{
}


//------------------------------------------------------------------------------
ClientTeam::~ClientTeam()
{
    if (beacon_boundary_) delete beacon_boundary_;
}


//------------------------------------------------------------------------------
void ClientTeam::createBoundary(physics::OdeCollisionSpace * world_space)
{    
    if (beacon_boundary_) return;
    beacon_boundary_ = new BeaconBoundaryClient(world_space,
                                                s_params.get<std::string>(config_name_ + ".boundary_tex_name") );

//    beacon_boundary_->setDrawLosHints(s_params.get<bool>(TEAM_CONFIG_NAME[id_] + ".draw_los_hints"));
}


//------------------------------------------------------------------------------
BeaconBoundaryClient * ClientTeam::getBoundary()
{
    return beacon_boundary_;
}

#endif
