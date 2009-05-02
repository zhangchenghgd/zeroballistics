

#include "RigidBodyVisual.h"

#include <limits>

#include <osg/MatrixTransform>
#include <osg/BlendFunc>
#include <osg/Material>

#include "RigidBody.h"
#include "SceneManager.h"
#include "ParameterManager.h"
#include "UtilsOsg.h"
#include "Scheduler.h"
#include "Paths.h"
#include "OsgNodeWrapper.h"
#include "TerrainDataClient.h"

#undef min
#undef max

bool                               RigidBodyVisual::render_targets_ = false;
const terrain::TerrainDataClient * RigidBodyVisual::terrain_data_   = NULL;


///< Lightmap values aboce this value won't influence the diffuse
///lighting of objects above it
const float SHADOW_THRESHOLD = 0.4f;
const float SHADOW_TRACKING_SPEED = 0.02f;

//------------------------------------------------------------------------------
RigidBodyVisual::~RigidBodyVisual()
{
    if (target_visual_.get())
    {
        DeleteNodeVisitor delete_visual(target_visual_.get());
        s_scene_manager.getRootNode()->accept(delete_visual);
    }
}


//------------------------------------------------------------------------------
void RigidBodyVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{
    GameObjectVisual::operator()(node, nv);

    updateTransform();
}


//------------------------------------------------------------------------------
void RigidBodyVisual::updateTransform()
{
    RigidBody * rb = (RigidBody*)object_;

    bool has_proxy = rb->getProxy();

    osg_wrapper_->setTransform(rb->getTransform(has_proxy));

    if (has_proxy)
    {
        // Handle render target
        if ((bool)target_visual_.get() !=  render_targets_)
        {
            // Cannot insert / delete node inside update traversal -> schedule with zero delay
            s_scheduler.addEvent(SingleEventCallback(this, &RigidBodyVisual::updateTargetVisual),
                                 0.0f, NULL,
                                 "updateTargetVisual",
                                 &fp_group_);
        }    
        if (target_visual_.get())
        {
            // Ordinary update of target visual
            target_visual_->setMatrix(matGl2Osg(rb->getTransform()));        
        }
    }

    setBrightnessFromTerrain();    
}



//------------------------------------------------------------------------------
void RigidBodyVisual::setTerrainData(const terrain::TerrainDataClient * terrain_data)
{
    terrain_data_ = terrain_data;
}


//------------------------------------------------------------------------------
void RigidBodyVisual::toggleRenderTargets()
{
    render_targets_ ^= 1;
}


//------------------------------------------------------------------------------
RigidBodyVisual::RigidBodyVisual() :
    material_(new osg::Material())    
{
}


//------------------------------------------------------------------------------
void RigidBodyVisual::onModelChanged()
{
    RigidBody * rb = dynamic_cast<RigidBody*>(object_);
    assert(rb);

    if (rb->isStatic())
    {
        // In case only the model was changed (e.g. conquered beacon),
        // there will be no transform update, so do it manually now
        updateTransform();
    } else
    {
        osg_wrapper_->getOsgNode()->setUpdateCallback(this);
    }

    if (osg_wrapper_->getOsgNode())
    {
        osg_wrapper_->getOsgNode()->getOrCreateStateSet()->setAttribute(material_.get());
    }
}


//------------------------------------------------------------------------------
/**
 *  Creates the osg model for the rigid body.
 */
void RigidBodyVisual::onGameObjectSet()
{
    assert(dynamic_cast<RigidBody*>(object_));
    object_->addObserver(ObserverCallbackFun0(this, &RigidBodyVisual::onRigidBodyInit),
                         RBE_INITIALIZATION_FINISHED,
                         &fp_group_);    
}



//------------------------------------------------------------------------------
void RigidBodyVisual::onRigidBodyInit()
{
    RigidBody * rb = dynamic_cast<RigidBody*>(object_);
    assert(rb);

    fp_group_.deregister(ObserverFp(&fp_group_, object_, RBE_INITIALIZATION_FINISHED));    

    setModel(rb->getName());

    if (rb->isStatic())
    {
        // Only update when object's position is explicitly set.
        rb->addObserver(ObserverCallbackFun0(this, &RigidBodyVisual::updateTransform),
                        RBE_POSITION_SET_EXTERNALLY,
                        &fp_group_);
    }
}
    





//------------------------------------------------------------------------------
/**
 *  Creates or removes target_visual_, dependent on render_targets_
 *  and on whether the body is static.
 */
void RigidBodyVisual::updateTargetVisual(void*)
{
    if (!render_targets_)
    {
        if (!target_visual_.get()) return;
        
        DeleteNodeVisitor delete_visual(target_visual_.get());
        s_scene_manager.getRootNode()->accept(delete_visual);

        assert(target_visual_->referenceCount() == 1);
        target_visual_ = NULL;
    } else
    {
        if (target_visual_.get()) return; // gets scheduled twice for shadow blockers
        if (((RigidBody*)object_)->isStatic()) return;

        target_visual_ = new osg::MatrixTransform();
        target_visual_->setName(((RigidBody*)object_)->getName() + "-TARGET");
        target_visual_->addChild(osg_wrapper_->getOsgNode()->getChild(0));

        osg::ref_ptr<osg::StateSet> blend_state = target_visual_->getOrCreateStateSet();
        blend_state->setAttributeAndModes(new osg::BlendFunc(osg::BlendFunc::ONE,osg::BlendFunc::ONE),
                                          osg::StateAttribute::ON );

        s_scene_manager.addNode(target_visual_.get());
    }
}



//------------------------------------------------------------------------------
void RigidBodyVisual::setBrightnessFromTerrain()
{
    RigidBody * rb = (RigidBody*)object_;

    if (rb->isStatic())
    {
        material_->setDiffuse(osg::Material::FRONT, osg::Vec4(1.0, 1.0, 1.0, 1.0));
    } else
    {
        const Matrix & transform = rb->getTransform();
        assert(terrain_data_);
        Color lm_color = terrain_data_->getColor(transform.getTranslation().x_,
                                                 transform.getTranslation().z_);

        float brightness = lm_color.getBrightness();
        // We don't want to follow the terrain shades, just the shadows
        if (brightness > SHADOW_THRESHOLD) brightness = 1.0f;

        // We don't want to get darker than ambient so lighting is still
        // done.
        brightness = std::max(brightness, s_scene_manager.getAmbient());
    
        // Don't set directly because of discrete values in lm (looks jerky)
        osg::Vec4 cur_color = material_->getDiffuse(osg::Material::FRONT) * (1.0f - SHADOW_TRACKING_SPEED);
        cur_color += osg::Vec4(brightness,
                               brightness,
                               brightness, 1.0f) * SHADOW_TRACKING_SPEED;

        material_->setDiffuse(osg::Material::FRONT, cur_color);
    }
}
