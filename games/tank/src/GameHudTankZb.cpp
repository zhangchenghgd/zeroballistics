
#include "GameHudTankZb.h"

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
#include "Minimap.h"
#include "WeaponSystem.h"
#include "ParameterManager.h"
#include "Datatypes.h"
#include "GameLogicClientCommon.h" // used for time limit display

#undef min
#undef max


//------------------------------------------------------------------------------
GameHudTankZb::GameHudTankZb(PuppetMasterClient * master, const std::string & config_file) :
    GameHudTank(master, config_file)
{
    geode_->addDrawable(new HudTextureElement("back1"));

    osg::Drawable * overlay = new HudTextureElement("back2");
    geode_->addDrawable(overlay);
    overlay->getOrCreateStateSet()->setRenderBinDetails(BN_HUD_OVERLAY, "RenderBin");

    setupTankTurretHud("");
    
    // Reload Bar
    reload_bar_ = new HudAlphaBar("reload_bar");
    geode_->addDrawable(reload_bar_.get());
    reload_bar_skill1_ = new HudBar("reload_bar_skill1");
    geode_->addDrawable(reload_bar_skill1_.get());
    reload_bar_skill2_ = new HudBar("reload_bar_skill2");
    geode_->addDrawable(reload_bar_skill2_.get());
    
    
    // Health display
    health_bar_ = new HudAlphaBar("health_bar");
    geode_->addDrawable(health_bar_.get());
    health_text_ = new HudTextElement("health");
    geode_->addDrawable(health_text_.get());


    // Upgrade Bars, lights
    for (unsigned i=0; i<NUM_UPGRADE_CATEGORIES; ++i)
    {
        upgrade_bar_[i] = new HudAlphaBar("upgrade_bar" + toString(i+1));
        geode_->addDrawable(upgrade_bar_[i].get());
        upgrade_light_[i] = new HudTextureElement("upgrade_light" + toString(i+1));
        upgrade_light_[i]->getOrCreateStateSet()->setRenderBinDetails(BN_HUD_OVERLAY, "RenderBin");
    }


    // Setup hit feedback
    osg::Geode * hit_feedback_geode = new osg::Geode;
    hit_feedback_ = new HudTextureElement("hit_feedback");

    // blend out stateset
    osg::StateSet * hit_feedback_stateset = hit_feedback_geode->getOrCreateStateSet();
    hit_feedback_stateset->setAttribute(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    hit_feedback_stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
    hit_feedback_stateset->setRenderingHint(BN_HUD_OVERLAY);

    hit_feedback_geode->addDrawable(hit_feedback_.get());
    group_->addChild(hit_feedback_geode);
    

    // Frags
    score_text_ = new HudTextElement("score");
    geode_->addDrawable(score_text_.get());

    // Time limit display
    hud_time_limit_ = new HudTextElement("time_limit");
    geode_->addDrawable(hud_time_limit_.get());

    time_limit_bar_ = new HudAlphaBar(std::string("time_limit_bar"));
    geode_->addDrawable(time_limit_bar_.get());
    
    onWindowResized();
}

//------------------------------------------------------------------------------
GameHudTankZb::~GameHudTankZb()
{
}

//------------------------------------------------------------------------------
void GameHudTankZb::update(float dt)
{
    GameHudTank::update(dt);

    
    assert(puppet_master_);

    GameLogicClientCommon * game_logic = (GameLogicClientCommon*)puppet_master_->getGameLogic();

    PlayerScore * tracked_score = game_logic->getScore().getPlayerScore(game_logic->getTrackedPlayer());
    if (!tracked_score) return;
    Tank * tank = dynamic_cast<Tank*>(tracked_score->getPlayer()->getControllable());

    if(tank)
    {
        float yaw,pitch;
        tank->getProxyTurretPos(yaw,pitch);
        osg::Matrix m = body_transform_->getMatrix();
        m.setRotate(osg::Quat(-yaw,osg::Vec3(0.0,0.0,1.0)));
        body_transform_->setMatrix(m);

        health_bar_->setValue((float)tank->getHitpoints() /
                              (float)tank->getMaxHitpoints());

        // show info for weapons ammo
        WeaponSystem ** weapons = tank->getWeaponSystems();
        reload_bar_       ->setValue(1.0f - weapons[0]->getCooldownStatus());
        reload_bar_skill1_->setValue(weapons[2]->getCooldownStatus());
        reload_bar_skill2_->setValue(weapons[3]->getCooldownStatus());

//        ammo_text_->setText(toString(weapons[0]->getAmmo()));
        


        // Upgrade bars & lights
        uint16_t current_points = tracked_score->upgrade_points_;
        for(unsigned c=0; c < NUM_UPGRADE_CATEGORIES; ++c)
        {
            UPGRADE_CATEGORY category = (UPGRADE_CATEGORY)c;

            if(tracked_score->isUpgradeCategoryLocked(category))
            {
                enableUpgradeLight(category, false);
                upgrade_bar_[c]->setValue(1.0f);

                osg::Vec4Array * osg_color = (osg::Vec4Array*)upgrade_bar_[c]->getColorArray();
                if ((*osg_color)[0].y() != 0.0f)
                {
                    (*osg_color)[0].set(1, 0, 0, 0.5);
                    upgrade_bar_[c]->setColorArray(osg_color);
                }
            } else
            {
                uint16_t needed_points = tracked_score->getNeededUpgradePoints(category);
                enableUpgradeLight(category, current_points >= needed_points);
                upgrade_bar_[c]->setValue((float)current_points / needed_points);

                osg::Vec4Array * osg_color = (osg::Vec4Array*)upgrade_bar_[c]->getColorArray();
                if ((*osg_color)[0].y() != 1.0f)
                {
                    (*osg_color)[0].x() = 1.0f;
                    (*osg_color)[0].y() = 1.0f;
                    (*osg_color)[0].z() = 1.0f;
                    upgrade_bar_[c]->setColorArray(osg_color);
                }
            }
        }

        score_text_->setText(toString(tracked_score->kills_));
        health_text_->setText(toString(tank->getHitpoints()));
    }    

    

    // Update hit feedback
    setHitFeedbackBlend(std::max(getHitFeedbackBlend() - (dt / BLEND_OUT_HIT_MARKER_TIME), 0.0f));


    // update remaining time
    float time_left = game_logic->getScore().getTimeLeft();
    float perc = 1.0f;
    // time_left / s_params.get<float>("server.settings.time_limit");
    // XXXX change this as soon as server->client parameter
    // transmission works

    unsigned minutes = (unsigned)(time_left / 60.0f);
    time_left -= minutes*60;
    unsigned seconds = (unsigned)time_left;
    time_left -= seconds;
    
    std::ostringstream strstr;
    strstr << std::setw(3) << std::setfill(' ') << minutes << ":";
    strstr << std::setw(2) << std::setfill('0') << seconds;
    hud_time_limit_->setText(strstr.str());

    time_limit_bar_->setValue(perc);
}


//------------------------------------------------------------------------------
void GameHudTankZb::onWindowResized()
{
    GameHudTank::onWindowResized();
    
    Vector2d pos = HudTextureElement::getScreenCoords("attitude_display");
    float size   = HudTextureElement::alignAndGetSize("attitude_display", pos);

    osg::Matrix scale_n_offset;
    scale_n_offset.makeScale(osg::Vec3(size,
                                       size, 1.0f));

    scale_n_offset.setTrans(pos.x_, pos.y_, 0.0f);
    tank_orientation_->setMatrix(scale_n_offset);

    hud_time_limit_->recalcTextPos();
    score_text_->recalcTextPos();
    health_text_->recalcTextPos();
//    ammo_text_->recalcTextPos();
}


//------------------------------------------------------------------------------
void GameHudTankZb::activateHitFeedback()
{
    setHitFeedbackBlend(1.0f);
}


//------------------------------------------------------------------------------
void GameHudTankZb::setAttitudeTexture(const std::string & tex)
{
    DeleteNodeVisitor v1(body_transform_.get());
    DeleteNodeVisitor v2(tank_orientation_.get());
    s_scene_manager.getRootNode()->accept(v1);
    s_scene_manager.getRootNode()->accept(v2);

    assert(body_transform_  ->referenceCount() == 1);
    assert(tank_orientation_->referenceCount() == 1);
    body_transform_   = NULL;
    tank_orientation_ = NULL;
    
    setupTankTurretHud(tex);

    onWindowResized(); // XXX HACK?
}


//------------------------------------------------------------------------------
void GameHudTankZb::setupTankTurretHud(const std::string & tex)
{
    // Screen position transform    
    tank_orientation_ = new osg::MatrixTransform;
    tank_orientation_->setName("HUD tank screen pos");
    
    // tank body transform
    body_transform_ = new osg::MatrixTransform;    
    body_transform_->setName("HUD tank body");

    tank_orientation_->addChild(body_transform_.get());
    group_->addChild(tank_orientation_.get());
    
    if (!tex.empty())
    {
        osg::ref_ptr<osg::Geode> turret_geode = new osg::Geode;    
        turret_geode->addDrawable(new HudTextureElement("attitude_turret", "data/textures/hud/turret_" + tex + ".dds"));
        turret_geode->setName("Turret");
        tank_orientation_->addChild(turret_geode.get());


        osg::ref_ptr<osg::Geode> body_geode = new osg::Geode;
        body_geode->addDrawable(new HudTextureElement("attitude_tank", "data/textures/hud/tank_" + tex + ".dds"));
        body_geode->setName("Body");
        body_transform_->addChild(body_geode.get());
    }
}



//------------------------------------------------------------------------------
void GameHudTankZb::enableUpgradeLight(UPGRADE_CATEGORY category, bool e)
{
    if (!e && upgrade_light_[category]->getNumParents() == 1)
    {
        ((osg::Geode*)upgrade_light_[category]->getParent(0))->removeDrawable(upgrade_light_[category].get());
    } else if (e && upgrade_light_[category]->getNumParents() == 0)
    {
        geode_->addDrawable(upgrade_light_[category].get());
    }
}



//------------------------------------------------------------------------------
void GameHudTankZb::setHitFeedbackBlend(float b)
{
    osg::Vec4Array * osg_color =  (osg::Vec4Array*)hit_feedback_->getColorArray();
    (*osg_color)[0].set(1, 1, 1, b);
}

//------------------------------------------------------------------------------
float GameHudTankZb::getHitFeedbackBlend() const
{
    osg::Vec4Array * osg_color =  (osg::Vec4Array*)hit_feedback_->getColorArray();
    
    return (float)(*osg_color)[0].a();
}
