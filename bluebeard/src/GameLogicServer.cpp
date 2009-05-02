#include "GameLogicServer.h"

#include "GameState.h"
#include "PuppetMasterServer.h"
#include "PlayerInput.h"

bool GameLogicServer::create_visuals_ = false;


//------------------------------------------------------------------------------
GameLogicServer::GameLogicServer() :
    puppet_master_(NULL)
{
}

//------------------------------------------------------------------------------
GameLogicServer::~GameLogicServer()
{
}


//------------------------------------------------------------------------------
/**
 *  Instead of constructor because we are created by factory.
 */
void GameLogicServer::init(PuppetMasterServer * master)
{
    assert(!puppet_master_);
    puppet_master_ = master;
}


//------------------------------------------------------------------------------
PuppetMasterServer * GameLogicServer::getPuppetMaster()
{
    return puppet_master_;
}


//------------------------------------------------------------------------------
void GameLogicServer::setCreateVisuals(bool b)
{
    create_visuals_ = b;
}


//------------------------------------------------------------------------------
bool GameLogicServer::isCreateVisualsEnabled()
{
    return create_visuals_;
}
