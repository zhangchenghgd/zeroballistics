
#include "GameHudTankSoccer.h"

#include <limits>

#include <osg/Geode>
#include <osg/Group>
#include <osg/Geometry>
#include <osg/NodeCallback>
#include <osg/BlendFunc>
#include <osg/BlendColor>

#include "SceneManager.h"
#include "PuppetMasterClient.h"
#include "Tank.h"
#include "HudBar.h"
#include "HudAlphaBar.h"
#include "HudTextElement.h"
#include "WeaponSystem.h"
#include "ParameterManager.h"
#include "Datatypes.h"
#include "Paths.h"
#include "GameLogicClientCommon.h" // used for time limit display
#include "TeamSoccer.h"

#undef min
#undef max



//------------------------------------------------------------------------------
GameHudTankSoccer::GameHudTankSoccer(PuppetMasterClient * master, const std::string & config_file) :
    GameHudTank(master, config_file)
{
    geode_->addDrawable(new HudTextureElement("timescore_back"));

    
    // Reload Bar
    reload_bar_ = new HudBar("reload_bar");
    geode_->addDrawable(reload_bar_.get());

    reload_bar_skill2_ = new HudBar("reload_bar_skill2");
    geode_->addDrawable(reload_bar_skill2_.get());
    
    
    // Health display
    health_bar_ = new HudBar("health_bar");
    geode_->addDrawable(health_bar_.get());

    // Time limit display
    time_limit_ = new HudTextElement("time_limit");
    geode_->addDrawable(time_limit_.get());

    // Score display
    score_ = new HudTextElement("score");
    geode_->addDrawable(score_.get());
    
    onWindowResized();
}

//------------------------------------------------------------------------------
GameHudTankSoccer::~GameHudTankSoccer()
{
}

//------------------------------------------------------------------------------
void GameHudTankSoccer::update(float dt)
{
    GameHudTank::update(dt);
    
    assert(puppet_master_);

    GameLogicClientCommon * game_logic = (GameLogicClientCommon*)puppet_master_->getGameLogic();

    PlayerScore * tracked_score = game_logic->getScore().getPlayerScore(game_logic->getTrackedPlayer());
    if (!tracked_score) return;
    Tank * tank = dynamic_cast<Tank*>(tracked_score->getPlayer()->getControllable());

    if(tank)
    {

        health_bar_->setValue((float)tank->getHitpoints() /
                              (float)tank->getMaxHitpoints());

        // show info for weapons ammo
        WeaponSystem ** weapons = tank->getWeaponSystems();
        reload_bar_       ->setValue(1.0f - weapons[0]->getCooldownStatus());
        reload_bar_skill2_->setValue(1.0f - weapons[3]->getCooldownStatus());


    }    


    // update remaining time
    float time_left = game_logic->getScore().getTimeLeft();

    unsigned minutes = (unsigned)(time_left / 60.0f);
    time_left -= minutes*60;
    unsigned seconds = (unsigned)time_left;
    time_left -= seconds;
    
    std::ostringstream strstr;
    strstr << std::setw(3) << std::setfill(' ') << minutes << ":";
    strstr << std::setw(2) << std::setfill('0') << seconds;
    time_limit_->setText(strstr.str());


    // update score display
    unsigned goals[2];
    goals[0] = game_logic->getScore().getTeamScore(TEAM_ID_BLUE)->score_;
    goals[1] = game_logic->getScore().getTeamScore(TEAM_ID_RED )->score_;

    std::string score_text = toString(goals[0]) + ":" + toString(goals[1]);
    if (goals[1] < 10) score_text += "  ";
    score_->setText(score_text);
}


//------------------------------------------------------------------------------
void GameHudTankSoccer::onWindowResized()
{
    GameHudTank::onWindowResized();
    
    time_limit_->recalcTextPos();
    score_->recalcTextPos();
}



