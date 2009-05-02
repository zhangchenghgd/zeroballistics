
#include "AIPlayer.h"

#include "PuppetMasterServer.h"


//------------------------------------------------------------------------------
AIPlayer::AIPlayer(PuppetMasterServer * puppet_master) :
    pid_(UNASSIGNED_SYSTEM_ADDRESS),
    sv_player_(NULL),
    puppet_master_(puppet_master)
{
}

//------------------------------------------------------------------------------
AIPlayer::~AIPlayer()
{

}

//------------------------------------------------------------------------------
/***
 *  num_ai_players used to get different system addresses for each ai player
 */
void AIPlayer::addPlayer(unsigned num_ai_players)
{
}


//------------------------------------------------------------------------------
void AIPlayer::removePlayer()
{ 
}

//------------------------------------------------------------------------------
void AIPlayer::handleLogic(float dt)
{
}




