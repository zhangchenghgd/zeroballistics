
#include "GameHudTank.h"

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

const float BLEND_OUT_HIT_MARKER_TIME = 0.5f;


//------------------------------------------------------------------------------
/**
 *  Avoid cyclic reference headaches...
 */
class GameHudTankUpdater : public osg::NodeCallback
{
public:
    GameHudTankUpdater(GameHudTank * hud) : hud_(hud) {}

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv)
        {
            hud_->update(s_scheduler.getLastFrameDt());
        }
protected:
    GameHudTank * hud_;
};



//------------------------------------------------------------------------------
GameHudTank::GameHudTank(PuppetMasterClient * master) :
    minimap_(master),
    puppet_master_(master)
{
    // Setup root group / geode
    geode_ = new osg::Geode;
    geode_->setName("Hud root geode");

    group_ = new osg::Group;
    group_->setName("Hud root group");
    group_->addChild(geode_.get());
    group_->setUpdateCallback(new GameHudTankUpdater(this));
    s_scene_manager.addHudNode(group_.get());


    geode_->addDrawable(new HudTextureElement("back1"));

    osg::Drawable * overlay = new HudTextureElement("back2");
    geode_->addDrawable(overlay);
    overlay->getOrCreateStateSet()->setRenderBinDetails(BN_HUD_OVERLAY, "RenderBin");

    setupHitMarkerHud();
    setupTankTurretHud("");
    
    // Reload Bar
    reload_bar_ = new HudAlphaBar("reload_bar");
    geode_->addDrawable(reload_bar_.get());
//    ammo_text_ = new HudTextElement("ammo");
//    geode_->addDrawable(ammo_text_.get());
    
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


    // Frags
    score_text_ = new HudTextElement("score");
    geode_->addDrawable(score_text_.get());

    // Time limit display
    hud_time_limit_ = new HudTextElement("time_limit");
    geode_->addDrawable(hud_time_limit_.get());

    time_limit_bar_ = new HudAlphaBar(std::string("time_limit_bar"));
    geode_->addDrawable(time_limit_bar_.get());



    s_scene_manager.addObserver(ObserverCallbackFun0(this, &GameHudTank::onWindowResized),
                                SOE_RESOLUTION_CHANGED,
                                &fp_group_);
    
    onWindowResized();
}

//------------------------------------------------------------------------------
GameHudTank::~GameHudTank()
{
    DeleteNodeVisitor delete_group(group_.get());
    s_scene_manager.getRootNode()->accept(delete_group);

    assert(group_->referenceCount() == 1); // we should hold the last
                                          // reference.
}

//------------------------------------------------------------------------------
void GameHudTank::update(float dt)
{
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
        reload_bar_->setValue(weapons[1]->getCooldownStatus());

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

    minimap_.update();


    // Update hit markers
    for(unsigned m=0; m < NUM_HIT_MARKERS; m++)
    {
        setHitMarkerBlend(m, std::max(getHitMarkerBlend(m) - (dt / BLEND_OUT_HIT_MARKER_TIME),
                                      0.0f));
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
void GameHudTank::enable(bool e)
{
    group_->setNodeMask(e ? NODE_MASK_VISIBLE : NODE_MASK_INVISIBLE);

    minimap_.enable(e);
}


//------------------------------------------------------------------------------
Minimap * GameHudTank::getMinimap()
{
    return &minimap_;
}

//------------------------------------------------------------------------------
/**
 *  \param pos The position in world space which should be marked.
 */
void GameHudTank::activateHitMaker(const Vector & pos)
{
    Tank * tank = dynamic_cast<Tank*>(puppet_master_->getLocalPlayer()->getControllable());
    if(tank == NULL) return;

    
    // First calculate the angle to the marked position w.r.t. the
    // tank body
    Vector world_dir = tank->getPosition() - pos;
    Vector dir = tank->getTransform().transformVectorInv(world_dir);
    dir.y_ = 0.0;

    // Now correct for the turret viewing direction
    float yaw, pitch;
    tank->getProxyTurretPos(yaw, pitch);
    Vector view_dir(sin(yaw), 0, cos(yaw));

    // angle between two vectors, gives us an angle where the hit occured relativ to
    // the view direction
    float phi = acosf( vecCosAlpha(&dir, &view_dir) );
            
    // test wether dir lies left or right from view_dir, to compute a 0-360 angle range
    if((view_dir.z_*dir.x_ - view_dir.x_*dir.z_) > 0.0) phi = 2*PI - phi;


    // We have 8 possible sectors to be marked, calculate which one
    // the direction lies in
    unsigned sector = (int) ((phi + PI/8.0f) / (PI*0.25f)) % 8;

    setHitMarkerBlend(sector>>1, 1.0f);
    if (sector & 1) setHitMarkerBlend(((sector>>1)+1)%4, 1.0f);
}

//------------------------------------------------------------------------------
void GameHudTank::activateHitFeedback()
{
    setHitFeedbackBlend(1.0f);
}

//------------------------------------------------------------------------------
void GameHudTank::onWindowResized()
{
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
void GameHudTank::setAttitudeTexture(const std::string & tex)
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
void GameHudTank::setCrosshair(const std::string & crosshair_section)
{
    // remove old crosshair
    geode_->removeDrawable(crosshair_.get());

    // set new crosshair (old should be deleted due to ref ptr)
    crosshair_ = new HudTextureElement(crosshair_section);

    // set new crosshair on screen
    geode_->addDrawable(crosshair_.get());
}

//------------------------------------------------------------------------------
void GameHudTank::setupTankTurretHud(const std::string & tex)
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
void GameHudTank::setupHitMarkerHud()
{  
    osg::ref_ptr<osg::Geode> hit;
    osg::MatrixTransform * marker_transform;
    osg::StateSet * hit_marker_stateset;
    HudTextureElement * hit_texture; 

    const float mid = 0.5f;
    const float distance = 0.1f;

    osg::Vec3 marker_pos[NUM_HIT_MARKERS] = {
        osg::Vec3(mid,          mid+distance,   0.0f),
        osg::Vec3(mid+distance, mid,            0.0f),
        osg::Vec3(mid,          mid-distance,   0.0f),
        osg::Vec3(mid-distance, mid,            0.0f) 
    };

    // top
    for(unsigned c=0; c < NUM_HIT_MARKERS; c++)
    {
        hit = new osg::Geode;

        hit_texture = new HudTextureElement("hit_marker");
        hit->addDrawable(hit_texture);
        hit_marker_[c] = hit_texture;

        marker_transform = new osg::MatrixTransform;    
        osg::Matrix trans_n_rotate;

        trans_n_rotate.makeTranslate(marker_pos[c]);
        trans_n_rotate.setRotate(osg::Quat(osg::DegreesToRadians(c * -90.0f),osg::Vec3(0.0,0.0,1.0)));

        marker_transform->setMatrix(trans_n_rotate);
        marker_transform->setName("HUD marker pos");

        marker_transform->addChild(hit.get());

        // blend out stateset
        hit_marker_stateset = hit->getOrCreateStateSet();
        hit_marker_stateset->setAttribute(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        hit_marker_stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
        hit_marker_stateset->setRenderingHint(BN_HUD_OVERLAY);

        group_->addChild(marker_transform);
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

}



//------------------------------------------------------------------------------
void GameHudTank::enableUpgradeLight(UPGRADE_CATEGORY category, bool e)
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
void GameHudTank::setHitMarkerBlend(unsigned pos, float b)
{
    assert(pos < NUM_HIT_MARKERS);

    osg::Vec4Array * osg_color =  (osg::Vec4Array*)hit_marker_[pos]->getColorArray();
    (*osg_color)[0].set(1, 1, 1, b);
}

//------------------------------------------------------------------------------
float GameHudTank::getHitMarkerBlend(unsigned pos) const
{
    assert(pos < NUM_HIT_MARKERS);

    osg::Vec4Array * osg_color =  (osg::Vec4Array*)hit_marker_[pos]->getColorArray();
    
    return (float)(*osg_color)[0].a();
}

//------------------------------------------------------------------------------
void GameHudTank::setHitFeedbackBlend(float b)
{
    osg::Vec4Array * osg_color =  (osg::Vec4Array*)hit_feedback_->getColorArray();
    (*osg_color)[0].set(1, 1, 1, b);
}

//------------------------------------------------------------------------------
float GameHudTank::getHitFeedbackBlend() const
{
    osg::Vec4Array * osg_color =  (osg::Vec4Array*)hit_feedback_->getColorArray();
    
    return (float)(*osg_color)[0].a();
}
