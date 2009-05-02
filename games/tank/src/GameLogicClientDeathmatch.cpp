
#include "GameLogicClientDeathmatch.h"

#include "AutoRegister.h"
#include "LevelData.h"
#include "GameHudTank.h"
#include "GameHud.h"
#include "GameLogicServerDeathmatch.h"
#include "GUIScore.h"
#include "GUITankEquipment.h"
#include "InputHandler.h"
#include "ControllableVisual.h"
#include "TankMineVisual.h"
#include "SdlApp.h"

REGISTER_CLASS(GameLogicClient, GameLogicClientDeathmatch);


//------------------------------------------------------------------------------
GameLogicClientDeathmatch::GameLogicClientDeathmatch()
{
}

//------------------------------------------------------------------------------
void GameLogicClientDeathmatch::init(PuppetMasterClient * master)
{
    GameLogicClientCommon::init(master);
    
    team_.setId(0);
    team_.setConfigName("team_deathmatch");
    score_.addTeam(&team_);

    std::vector<Team*> team_for_sel(1, &team_);
    gui_score_->setTeams(team_for_sel);
}


//------------------------------------------------------------------------------
bool GameLogicClientDeathmatch::handleInput(PlayerInput & input)
{
    if (score_.getTeam(puppet_master_->getLocalPlayer()->getId()) == NULL)
    {
        // If we are assigned to no team, we are in spectator mode.
        if (input.fire3_)
        {
            s_log << "Requesting game start\n";
            sendTeamChangeRequest(0);

            if (s_params.get<bool>("server.logic_deathmatch.allow_spawn_stage_selection"))
            {
                handleRespawnCounter(0.0f);
            } else
            {
                sendRespawnRequest(0);
            }

            // don't let camera move...
            return true;
        }        
    } else
    {
        if (!s_params.get<bool>("server.logic_deathmatch.allow_spawn_stage_selection"))
        {
            if (!puppet_master_->getLocalPlayer()->getControllable() &&
                input.fire1_ &&
                !respawn_input_blocked_)
            {
                sendRespawnRequest(0);
                // immediately update msg
                handleRespawnCounter(0.0f);        
            }
        }
    }

    return GameLogicClientCommon::handleInput(input);
}


//------------------------------------------------------------------------------
void GameLogicClientDeathmatch::onRequestReady()
{
    s_log << Log::debug('l')
          << "onRequestReady\n";
    GameLogicClientCommon::onRequestReady();

    if (s_params.get<bool>("server.logic_deathmatch.allow_spawn_stage_selection"))
    {
        changeToSpawnSelectionCam(NULL);
        handleRespawnCounter(0.0f);
    } 

    std::string msg;
    msg += ("You are in spectator mode. Press '" +
            s_input_handler.getKeyForFunction("Drop Mine") +
            "' to enter the game.");
    puppet_master_->getHud()->setStatusLine(msg);

    s_app.captureMouse(true);

    // also update mine warning billboards
    TankMineVisual::setLocalPlayerId(puppet_master_->getLocalPlayer()->getId());
}


//------------------------------------------------------------------------------
void GameLogicClientDeathmatch::executeCustomCommand(unsigned type, RakNet::BitStream & args)
{
    switch (type)
    {
    case CSCTDM_GAME_WON:
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
void GameLogicClientDeathmatch::onLevelObjectLoaded(const std::string & type,
                                                    const bbm::ObjectInfo & info)
{
    GameLogicClientCommon::onLevelObjectLoaded(type, info);

    std::string role;
    
    try
    {
        role = info.params_.get<std::string>("logic.role");
    } catch (ParamNotFoundException & e)
    {
        return;
    }
    
    if (role == "start_pos")
    {
        spawn_pos_.push_back(info.transform_);
    }
}


//------------------------------------------------------------------------------
void GameLogicClientDeathmatch::onTeamAssignmentChanged(Player * player)
{
    GameLogicClientCommon::onTeamAssignmentChanged(player);

    std::string msg;
    if (player == puppet_master_->getLocalPlayer())
    {
        msg = "You have joined the match.";
    } else
    {
        msg = player->getName() + " has joined the match.";
    }
    puppet_master_->getHud()->addMessage(msg, Color(0.0f,1.0f,0.0f));
}



//------------------------------------------------------------------------------
void GameLogicClientDeathmatch::onLocalControllableSet()
{
    GameLogicClientCommon::onLocalControllableSet();
}


//------------------------------------------------------------------------------
void GameLogicClientDeathmatch::onLocalPlayerKilled(Player * killer)
{
    if (s_params.get<bool>("server.logic_deathmatch.allow_spawn_stage_selection"))
    {
        GameLogicClientCommon::onLocalPlayerKilled(killer);
    } else
    {
        blockRespawnInput();
        
        // free camera is automatically used if there is no
        // controllable.
        setupDeathCam(killer ? killer->getControllable() : NULL,
                      puppet_master_->getLocalPlayer()->getControllable());

        startRespawnCounter();
    }
}


//------------------------------------------------------------------------------
void GameLogicClientDeathmatch::handleMinimapIcon(RigidBody * object, bool force_reveal)
{
    if (object->getType() == "Tank" &&
        (object->getOwner() == puppet_master_->getLocalPlayer()->getId() || force_reveal))
    {
        hud_->getMinimap()->addIcon(object, "tank_" + object->getName() + ".dds");
    }
}


//------------------------------------------------------------------------------
Matrix GameLogicClientDeathmatch::selectValidSpawnStage(int delta)
{
    assert(!spawn_pos_.empty());
    if(selected_spawn_stage_ >= spawn_pos_.size()) selected_spawn_stage_ = 0;

    selected_spawn_stage_ = (selected_spawn_stage_+spawn_pos_.size()+delta) % spawn_pos_.size();

    return spawn_pos_[selected_spawn_stage_];
}



//------------------------------------------------------------------------------
GameLogicClient * GameLogicClientDeathmatch::create()
{
    return new GameLogicClientDeathmatch();
}

//------------------------------------------------------------------------------
void GameLogicClientDeathmatch::gameWon (RakNet::BitStream & args)
{
    GameLogicClientCommon::gameWon();
    
    SystemAddress winner_id;
    args.Read(winner_id);

    std::string game_info_label;

    Player * winning_player;
    if (winner_id == puppet_master_->getLocalPlayer()->getId())
    {
        winning_player = puppet_master_->getLocalPlayer();
        game_info_label = "Congratulations! You are the winner!";
    } else
    {
        winning_player = puppet_master_->getRemotePlayer(winner_id);
        if (!winning_player)
        {
            game_info_label = "Round draw.";
        } else
        {
            game_info_label = winning_player->getName() + " has won the match!";
        }
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



    // winning camera
    if (winning_player && winning_player->getControllable())
    {
        const Vector OFFSET = Vector(0,1,3);
        ControllableVisual * visual = (ControllableVisual*)winning_player->getControllable()->getUserData();
        Matrix new_pos = visual->getTrackingPos(Vector(0,0,0));

        new_pos.getTranslation() += new_pos.transformVector(OFFSET);
        new_pos.loadOrientation(winning_player->getControllable()->getPosition() - new_pos.getTranslation(), Vector(0,1,0));
    
        spectator_camera_.trackPlayer(winning_player->getId());
        spectator_camera_.setMode(CM_TRACKING_3RD_CONSTANT_DIR);
        spectator_camera_.setTransform(new_pos);
    }    
}
