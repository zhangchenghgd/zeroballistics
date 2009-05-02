

#include "TankMineVisual.h"


#include "TankMine.h"
#include "SceneManager.h"
#include "AutoRegister.h"
#include "GameObject.h"
#include "OsgNodeWrapper.h"
#include "HudTextureElement.h"


REGISTER_CLASS(GameObjectVisual, TankMineVisual);

TEAM_ID       TankMineVisual::local_player_team_id_ = INVALID_TEAM_ID;
SystemAddress TankMineVisual::local_player_id_      = UNASSIGNED_SYSTEM_ADDRESS;

//------------------------------------------------------------------------------
TankMineVisual::~TankMineVisual()
{
}


//------------------------------------------------------------------------------
void TankMineVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{
    RigidBodyVisual::operator()(node, nv);

    TankMine * mine = (TankMine*)object_;
    
    bool warning_visible = (mine->getOwner()  == local_player_id_ ||
                            mine->getTeamId() == local_player_team_id_);

    warning_billboard_->setNodeMask(warning_visible ? NODE_MASK_VISIBLE : NODE_MASK_INVISIBLE);
}

//------------------------------------------------------------------------------
void TankMineVisual::setLocalPlayerTeam(TEAM_ID tid)
{
    local_player_team_id_ = tid;
    local_player_id_ = UNASSIGNED_SYSTEM_ADDRESS;
}

//------------------------------------------------------------------------------
void TankMineVisual::setLocalPlayerId(const SystemAddress & player_id)
{
    local_player_id_ = player_id;
    local_player_team_id_ = INVALID_TEAM_ID;
}

//------------------------------------------------------------------------------
TankMineVisual::TankMineVisual()
{
}


//------------------------------------------------------------------------------
void TankMineVisual::onModelChanged()
{
    RigidBodyVisual::onModelChanged();


    // do first-time stuff
    if (warning_billboard_.get() == NULL)
    {
        osg::Vec3 offset(0.0f, osg_wrapper_->getOsgNode()->getBound().radius() + 0.1f, 0.0f );
        osg::ref_ptr<HudTextureElement> warning_tex = new HudTextureElement("mine_warning");
        warning_tex->setDrawCallback(new DrawBillboardStyle(offset));
        warning_billboard_ = new osg::Geode();
        warning_billboard_->addDrawable(warning_tex.get());

        // Disable shader and lighting for billboard
        warning_billboard_->getOrCreateStateSet()->setAttribute(new osg::Program, osg::StateAttribute::OVERRIDE);
        warning_billboard_->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        warning_billboard_->getOrCreateStateSet()->setRenderBinDetails(BN_TRANSPARENT, "DepthSortedBin");
        warning_billboard_->getOrCreateStateSet()->setMode(GL_ALPHA_TEST, osg::StateAttribute::ON);
        warning_billboard_->setNodeMask(NODE_MASK_INVISIBLE);

        osg_wrapper_->getOsgNode()->addChild(warning_billboard_.get());
    }
}

