
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
#include "Paths.h"
#include "GameLogicClientCommon.h" // used for time limit display

#undef min
#undef max



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
GameHudTank::GameHudTank(PuppetMasterClient * master, const std::string & config_file) :
    puppet_master_(master)
{
    s_params.loadParameters(CONFIG_PATH + config_file);

    minimap_.reset(new Minimap(master));
    
    // Setup root group / geode
    geode_ = new osg::Geode;
    geode_->setName("Hud root geode");

    group_ = new osg::Group;
    group_->setName("Hud root group");
    group_->addChild(geode_.get());
    group_->setUpdateCallback(new GameHudTankUpdater(this));
    s_scene_manager.addHudNode(group_.get());

    setupHitMarkerHud();
    
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
void GameHudTank::enable(bool e)
{
    group_->setNodeMask(e ? NODE_MASK_VISIBLE : NODE_MASK_INVISIBLE);

    minimap_->enable(e);
}


//------------------------------------------------------------------------------
Minimap * GameHudTank::getMinimap()
{
    return minimap_.get();
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
void GameHudTank::onWindowResized()
{

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
void GameHudTank::update(float dt)
{
    assert(puppet_master_);

    GameLogicClientCommon * game_logic = (GameLogicClientCommon*)puppet_master_->getGameLogic();

    PlayerScore * tracked_score = game_logic->getScore().getPlayerScore(game_logic->getTrackedPlayer());
    if (!tracked_score) return;

    minimap_->update();

    // Update hit markers
    for(unsigned m=0; m < NUM_HIT_MARKERS; m++)
    {
        setHitMarkerBlend(m, std::max(getHitMarkerBlend(m) - (dt / BLEND_OUT_HIT_MARKER_TIME),
                                      0.0f));
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
