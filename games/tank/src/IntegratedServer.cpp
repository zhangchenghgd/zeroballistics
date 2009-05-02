

#include "IntegratedServer.h"


#include "NetworkServer.h"
#include "PuppetMasterServer.h"
#include "GameLogicServer.h"
#include "Scheduler.h"
#include "ParameterManager.h"


//------------------------------------------------------------------------------
IntegratedServer::IntegratedServer(uint32_t user_id, uint32_t session_key) :
    MetaTask("IntegratedServer")
{
    server_.reset(new NetworkServer());
    server_->setAuthData(user_id, session_key);
}

//------------------------------------------------------------------------------
IntegratedServer::~IntegratedServer()
{
}

//------------------------------------------------------------------------------
void IntegratedServer::onFocus()
{
    assert(false);
}


//------------------------------------------------------------------------------
void IntegratedServer::start()
{
    server_->start();

    server_->getPuppetMaster()->addObserver(ObserverCallbackFun0(this, &IntegratedServer::onGameFinished),
                                            PMOE_GAME_FINISHED,
                                            &fp_group_);
    server_->getPuppetMaster()->addObserver(ObserverCallbackFun0(this, &IntegratedServer::onLevelLoaded),
                                            PMOE_LEVEL_LOADED,
                                            &fp_group_);
    
    reloadLevel(NULL);
}

//------------------------------------------------------------------------------
NetworkServer * IntegratedServer::getServer()
{
    return server_.get();
}

//------------------------------------------------------------------------------
void IntegratedServer::onGameFinished()
{
    s_scheduler.addEvent(SingleEventCallback(this, &IntegratedServer::reloadLevel),
                         s_params.get<float>("server.logic.game_won_delay"),
                         NULL,
                         "IntegratedServer::reloadLevel",
                         &fp_group_);
}

//------------------------------------------------------------------------------
void IntegratedServer::onLevelLoaded()
{
    fp_group_.deregisterAllOfType(TaskFp());
}



//------------------------------------------------------------------------------
void IntegratedServer::reloadLevel(void*)
{
    server_->getPuppetMaster()->loadLevel(
        new HostOptions(s_params.get<std::string>("server.settings.level_name"),
                        s_params.get<std::string>("server.settings.type")));
}


