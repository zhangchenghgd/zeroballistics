

#include "GameLogicClient.h"


#include "PuppetMasterClient.h"
#include "PlayerInput.h"
#include "Controllable.h"



//------------------------------------------------------------------------------
GameLogicClient::GameLogicClient() :
    puppet_master_(NULL)
{
}


//------------------------------------------------------------------------------
GameLogicClient::~GameLogicClient()
{
}



//------------------------------------------------------------------------------
void GameLogicClient::init(PuppetMasterClient * master)
{
    puppet_master_ = master;
}

//------------------------------------------------------------------------------
PuppetMasterClient * GameLogicClient::getPuppetMaster()
{
    return puppet_master_;
}
