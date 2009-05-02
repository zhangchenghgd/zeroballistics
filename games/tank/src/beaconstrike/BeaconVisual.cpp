
#include "BeaconVisual.h"


#include <osg/Geode>
#include <osg/PolygonMode>

#include "Beacon.h"
#include "AutoRegister.h"
#include "SceneManager.h"
#include "HudBar.h"
#include "ParameterManager.h"
#include "OsgNodeWrapper.h"
#include "EffectManager.h"



REGISTER_CLASS(GameObjectVisual, BeaconVisual);


const std::string BEACON_OUTER_NODE_NAME = "rotate";

const std::string DEPLOYED_GROUP_NAME     = "deployed";
const std::string HOVERING_GROUP_NAME     = "hovering";
const std::string NOT_DEPLOYED_GROUP_NAME = "not_deployed";


const char * BEACON_HEAL_EFFECT_GROUP_NAME = "heal";


const float BEACON_ROTATE_SPEED = -2.0f;

const float BEACON_HOVER_AMPLITUDE = 0.05f;
const float BEACON_HOVER_OMEGA     = 2.0f;


//------------------------------------------------------------------------------
BeaconVisual::~BeaconVisual()
{
}


//------------------------------------------------------------------------------
/**
 *  Hover beacons up & down when deployed.
 */
void BeaconVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{
    RigidBodyVisual::operator()(node, nv);


    float time = (float)nv->getFrameStamp()->getSimulationTime();
    
    Beacon * beacon = (Beacon*)object_;
    if (beacon->isDeployed() || beacon->getState() == BS_HOVERING)
    {
        Vector pos = beacon->getTarget()->getPosition();
        pos.y_ += sin(time*BEACON_HOVER_OMEGA) * BEACON_HOVER_AMPLITUDE;
        osg_wrapper_->setPosition(pos);
    }



    
    // set health bars value depending on beacons health
    float health_percentage = (float)beacon->getHitpoints()/(float)beacon->getMaxHitpoints();
    health_bar_->setValue(health_percentage);
    
//     if (outer_node_.get() &&
//         beacon->getState() == BS_DEPLOYED ||
//         beacon->getState() == BS_HOVERING ||
//         beacon->getState() == BS_FIXED)
    {
        osg::Matrix mat;
        mat.makeRotate(BEACON_ROTATE_SPEED * time, 0.0f, 1.0f, 0.0f);
        outer_node_->setMatrix(mat);
    }






    
    // make billboard invisible on full health or if beacon is carried
    if (beacon->getHitpoints() == beacon->getMaxHitpoints() ||
        beacon->getState() == BS_CARRIED)
    {
        health_billboard_->setNodeMask(NODE_MASK_INVISIBLE);
    } else
    {
        health_billboard_->setNodeMask(NODE_MASK_VISIBLE);
    }

    if (prev_healing_ != beacon->isGainingHealth())
    {
        prev_healing_ = beacon->isGainingHealth();
        EnableGroupVisitor g(BEACON_HEAL_EFFECT_GROUP_NAME, prev_healing_);
        osg_wrapper_->getOsgNode()->accept(g);
    }
}


//------------------------------------------------------------------------------
void BeaconVisual::destroy()
{
    // Beacon will be deleted immediately, so add effect to osg root node
    s_scene_manager.getRootNode()->addChild(s_effect_manager.createEffect("beacon_explosion",
                                                                            osg_wrapper_->getPosition()));
}


//------------------------------------------------------------------------------
BeaconVisual::BeaconVisual() :
    prev_healing_(false)
{
}




//------------------------------------------------------------------------------
void BeaconVisual::onModelChanged()
{
    RigidBodyVisual::onModelChanged();

    Beacon * beacon = (Beacon*)object_;
    
    // do first-time stuff
    if (health_billboard_.get() == NULL)
    {
        beacon->addObserver(ObserverCallbackFun0(this, &BeaconVisual::onBeaconStateChanged),
                            BE_DEPLOYED_CHANGED,
                            &fp_group_);
        beacon->addObserver(ObserverCallbackFun0(this, &BeaconVisual::onBeaconStateChanged),
                            BE_HOVERING_CHANGED,
                            &fp_group_);
        beacon->addObserver(ObserverCallbackFun0(this, &BeaconVisual::onBeaconStateChanged),
                            BE_INSIDE_RADIUS_CHANGED,
                            &fp_group_);
        beacon->addObserver(ObserverCallbackFun0(this, &BeaconVisual::onBeaconStateChanged),
                            BE_CARRIED_CHANGED,
                            &fp_group_);



        osg::Vec3 offset(0.0f, beacon->getBodyGeom()->getRadius(), 0.0f );
        health_bar_ = new HudBar(false,"beacon_health_bar");
        health_bar_->setDrawCallback(new DrawBillboardStyle(offset));
        health_billboard_ = new osg::Geode();
        health_billboard_->addDrawable(health_bar_.get());

        // Disable shader and lighting for text
        health_billboard_->getOrCreateStateSet()->setAttribute(new osg::Program, osg::StateAttribute::OVERRIDE);
        health_billboard_->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        health_billboard_->getOrCreateStateSet()->setRenderBinDetails(BN_DEFAULT, "RenderBin");
        health_billboard_->setNodeMask(NODE_MASK_INVISIBLE);
    }

    osg_wrapper_->getOsgNode()->addChild(health_billboard_.get());


    std::vector<osg::Node*> outer_node_v = s_scene_manager.findNode(BEACON_OUTER_NODE_NAME, osg_wrapper_->getOsgNode());
    if (outer_node_v.size() != 1) throw Exception("Beacon contains zero or more than one outer nodes");
    
    outer_node_ = s_scene_manager.insertLocalTransformNode(outer_node_v[0]);

    onBeaconStateChanged();
}


//------------------------------------------------------------------------------
void BeaconVisual::onBeaconStateChanged()
{
    Beacon * beacon = (Beacon*)object_;

    bool deployed_group   = false;
    bool hover_group      = false;
    bool undeployed_group = false;

    if (beacon->isDeployed())
    {
        deployed_group = true;
    } else
    {
        if (beacon->isInsideRadius() && beacon->getState() != BS_DISPENSED)
        {
            hover_group = true;
        } else
        {
            undeployed_group = true;
        }
    }

    assert(!(deployed_group && hover_group));
    assert(!(deployed_group && undeployed_group));
    assert(!(hover_group    && undeployed_group));
    
    EnableGroupVisitor v1(HOVERING_GROUP_NAME,     hover_group);    
    EnableGroupVisitor v2(DEPLOYED_GROUP_NAME,     deployed_group);
    EnableGroupVisitor v3(NOT_DEPLOYED_GROUP_NAME, undeployed_group);

    osg_wrapper_->getOsgNode()->accept(v1);
    osg_wrapper_->getOsgNode()->accept(v2);
    osg_wrapper_->getOsgNode()->accept(v3);
}
