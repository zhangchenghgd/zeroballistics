
#include "GameLogicClientTeamDeathmatch.h"

#include "AutoRegister.h"
#include "LevelData.h"
#include "GameHudTank.h"
#include "GameHud.h"
#include "GUIScore.h"
#include "GUITeamSelect.h"
#include "GUITankEquipment.h"
#include "InputHandler.h"
#include "ControllableVisual.h"
#include "TankMineVisual.h"


#ifdef ZERO_BALLISTICS_MODE
REGISTER_CLASS(GameLogicClient, GameLogicClientTeamDeathmatch);
#endif

//------------------------------------------------------------------------------
GameLogicClientTeamDeathmatch::GameLogicClientTeamDeathmatch()
{
}

//------------------------------------------------------------------------------
void GameLogicClientTeamDeathmatch::init(PuppetMasterClient * master)
{
    GameLogicClientCommon::init(master);

    
    std::vector<Team*> team_for_sel;
    for (unsigned t=0; t<NUM_TEAMS_TDM; ++t)
    {
        team_[t].setId(t);
        team_[t].setConfigName(TEAM_CONFIG_TDM[t]);
        score_.addTeam(&team_[t]);

        team_for_sel.push_back(&team_[t]);
    }

    gui_teamselect_->setTeams(team_for_sel);
    gui_score_->setTeams(team_for_sel);
}


//------------------------------------------------------------------------------
bool GameLogicClientTeamDeathmatch::handleInput(PlayerInput & input)
{
    if (score_.getTeam(puppet_master_->getLocalPlayer()->getId()) != NULL &&
        !puppet_master_->getLocalPlayer()->getControllable() &&
        input.fire1_ &&
        !respawn_input_blocked_)
    {
        sendRespawnRequest(0);
        // immediately update msg
        handleRespawnCounter(0.0f);        
    }

    return GameLogicClientCommon::handleInput(input);
}


//------------------------------------------------------------------------------
void GameLogicClientTeamDeathmatch::onRequestReady()
{
    gui_teamselect_->show(true);
}


//------------------------------------------------------------------------------
void GameLogicClientTeamDeathmatch::executeCustomCommand(unsigned type, RakNet::BitStream & args)
{
    switch (type)
    {
    case CSCTTDM_GAME_WON:
        gameWon(args);
        break;
    default:
        GameLogicClientCommon::executeCustomCommand(type, args);
    }    
}


//------------------------------------------------------------------------------
/**
 *  Remember all spawning postions so they can be cycled in dev mode.
 */
void GameLogicClientTeamDeathmatch::onLevelObjectLoaded(const std::string & type,
                                                    const bbm::ObjectInfo & info)
{
    GameLogicClientCommon::onLevelObjectLoaded(type, info);
}


//------------------------------------------------------------------------------
void GameLogicClientTeamDeathmatch::onTeamAssignmentChanged(Player * player)
{
    if (player == puppet_master_->getLocalPlayer())
    {
        TEAM_ID team_id = score_.getTeamId(puppet_master_->getLocalPlayer()->getId());

        // also update mine warning billboards
        TankMineVisual::setLocalPlayerTeam(team_id);
        
        if (team_id != INVALID_TEAM_ID)
        {
        } else
        {
            setInputMode(IM_CONTROL_CAMERA);
        }
    }



    Team * team = score_.getTeam(player->getId());

    if (player == puppet_master_->getLocalPlayer())
    {
        if (team == NULL)
        {
            puppet_master_->getHud()->addMessage("You have been assigned to no team.");

            // Hide respawn counter
            show_respawn_text_ = false;
            handleRespawnCounter(0.0f);
        } else
        {
            show_respawn_text_ = true;
            puppet_master_->getHud()->addMessage(std::string("You have been assigned to ") +
                                                 team->getName() +
                                                 ".",
                                                 team->getColor());

            sendRespawnRequest(0);
        }
    } else if (puppet_master_->getLocalPlayer()->isLevelDataSet())
    {
        // Notification if other player changes team
        if (team)
        {
            puppet_master_->getHud()->addMessage(player->getName()   +
                                                 " has been assigned to " +
                                                 team->getName() +
                                                 ".",
                                                 team->getColor());
        } else
        {
            puppet_master_->getHud()->addMessage(player->getName() + " has been assigned to no team.");
        }
    }    
}



//------------------------------------------------------------------------------
void GameLogicClientTeamDeathmatch::onLocalControllableSet()
{
    GameLogicClientCommon::onLocalControllableSet();
}


//------------------------------------------------------------------------------
void GameLogicClientTeamDeathmatch::onLocalPlayerKilled(Player * killer)
{
    blockRespawnInput();
    
    // free camera is automatically used if there is no
    // controllable.
    setupDeathCam(killer ? killer->getControllable() : NULL,
                  puppet_master_->getLocalPlayer()->getControllable());

    startRespawnCounter();
}


//------------------------------------------------------------------------------
void GameLogicClientTeamDeathmatch::handleMinimapIcon(RigidBody * object, bool force_reveal)
{
    hud_->getMinimap()->removeIcon(object);

    Team * own_team = score_.getTeam(puppet_master_->getLocalPlayer()->getId());
    if (!own_team) return;
    
    if (object->getType() == "Tank" &&
        (score_.getTeamId(object->getOwner()) == own_team->getId() || force_reveal))
    {
        hud_->getMinimap()->addIcon(object, "tank_" + object->getName() + ".dds");
    }
}


//------------------------------------------------------------------------------
GameLogicClient * GameLogicClientTeamDeathmatch::create()
{
    return new GameLogicClientTeamDeathmatch();
}

//------------------------------------------------------------------------------
void GameLogicClientTeamDeathmatch::gameWon (RakNet::BitStream & args)
{
    GameLogicClientCommon::gameWon();

    TEAM_ID winner_team;
    args.Read(winner_team);

    std::string game_info_label;

    if (winner_team == INVALID_TEAM_ID)
    {
        game_info_label = "Round draw.";
    } else if (winner_team == score_.getTeamId(puppet_master_->getLocalPlayer()->getId()))
    {
        game_info_label = "Congratulations! You are the winner!";
    } else
    {
        game_info_label = "You have lost the match.";
    }

    s_log << Log::debug('l') << game_info_label;

    puppet_master_->getHud()->setStatusLine(game_info_label);

//    gui_score_->show(1.0f);

//     SoundSource * snd = NULL;
//     if(team_id == TEAM_ID_ATTACKER)
//     {
//         snd = s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.attacker_wins"),
//                                               s_soundmanager.getListenerInfo().position_);
//     } else
//     {
//         snd = s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.defender_wins"),
//                                               s_soundmanager.getListenerInfo().position_);
//     }
//     snd->setRolloffFactor(0.0f);


    spectator_camera_.setMode(CM_TRACKING_3RD_CONSTANT_DIR);

//     // winning camera
//     if (winning_player && winning_player->getControllable())
//     {
//         const Vector OFFSET = Vector(0,1,3);
//         ControllableVisual * visual = (ControllableVisual*)winning_player->getControllable()->getUserData();
//         Matrix new_pos = visual->getTrackingPos(Vector(0,0,0));

//         new_pos.getTranslation() += new_pos.transformVector(OFFSET);
//         new_pos.loadOrientation(winning_player->getControllable()->getPosition() - new_pos.getTranslation(), Vector(0,1,0));
    
//         spectator_camera_.trackPlayer(winning_player->getId());
//         spectator_camera_.setMode(CM_TRACKING_3RD_CONSTANT_DIR);
//         spectator_camera_.setTransform(new_pos);
//     }    
}
