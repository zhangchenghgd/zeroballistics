
#include "LodUpdater.h"

#include <limits>

#include "GameState.h"
#include "GameObject.h"
#include "GameObjectVisual.h"
#include "OsgNodeWrapper.h"
#include "SceneManager.h"
#include "ParameterManager.h"

const unsigned UPDATES_PER_CALL = 20;


const float HYSTERESIS_FACTOR = 0.95f;


/// If camera moves more than this amount(sqrt) in one frame, recalc
/// all lod levels.
const float MAX_CAMERA_JUMP_DISTANCE_SQR = 100.0f;

#undef min
#undef max

//------------------------------------------------------------------------------
LodUpdater::LodUpdater() :
    next_to_update_(0),
    lod_scale_      (s_params.getPointer<float>("client.graphics.lod_scale")),
    updater_enabled_(INVALID_TASK_HANDLE),
    last_camera_pos_(Vector(0.0f, 0.0f, 0.0f))
{
    updater_enabled_ =  s_scheduler.addFrameTask(PeriodicTaskCallback(this, &LodUpdater::update),
                        "LodUpdater::update",
                        &fp_group_);
}



//------------------------------------------------------------------------------
void LodUpdater::reset()
{
    osg_lod_nodes_.clear();
    next_to_update_ = 0;
}

//------------------------------------------------------------------------------
void LodUpdater::update(float dt)
{
    if(osg_lod_nodes_.empty()) return;

    Vector cur_camera_pos = s_scene_manager.getCamera().getPos();

    if ((cur_camera_pos - last_camera_pos_).lengthSqr() > MAX_CAMERA_JUMP_DISTANCE_SQR)
    {
        updateAll();
    } else
    {    
        for (unsigned i=0; i<UPDATES_PER_CALL; ++i)
        {
            if(next_to_update_ >= osg_lod_nodes_.size())
            {
                next_to_update_ = 0;
            }
            else
            {
                OsgNodeWrapper * node = osg_lod_nodes_[next_to_update_];
                updateNode(node);

                ++next_to_update_;
            }
        }
    }
    
    last_camera_pos_ = s_scene_manager.getCamera().getPos();
}


//------------------------------------------------------------------------------
/**
 *  Force update of all nodes. Useful if camera position changes by
 *  large amounts (respawn etc...)
 */
void LodUpdater::updateAll()
{
    for (unsigned i=0; i<osg_lod_nodes_.size(); ++i)
    {
        OsgNodeWrapper * node = osg_lod_nodes_[i];
        updateNode(node);
    }
}

//------------------------------------------------------------------------------
void LodUpdater::enable(bool v)
{
    if(v) // enable
    {
        if(updater_enabled_ != INVALID_TASK_HANDLE) return; // updater already active

        updater_enabled_ =  s_scheduler.addFrameTask(PeriodicTaskCallback(this, 
                            &LodUpdater::update),
                            "LodUpdater::update",
                            &fp_group_);        
    }
    else // disable
    {
        if(updater_enabled_ != INVALID_TASK_HANDLE) // if updater is active -> deactivate
        {
            s_scheduler.removeTask(updater_enabled_, &fp_group_);
            updater_enabled_ = INVALID_TASK_HANDLE;
        }
    }
}

//------------------------------------------------------------------------------
bool LodUpdater::isEnabled() const
{
    if(updater_enabled_ == INVALID_TASK_HANDLE)
    {
        return false;
    }
    else
    {
        return true;
    }
}



//------------------------------------------------------------------------------
void LodUpdater::addLodNode(OsgNodeWrapper * node)
{
    osg_lod_nodes_.push_back(node);
}

//------------------------------------------------------------------------------
void LodUpdater::removeLodNode(const OsgNodeWrapper * node)
{
    std::vector<OsgNodeWrapper*>::iterator it = osg_lod_nodes_.begin();
    for(; it != osg_lod_nodes_.end(); it++)
    {
        if( (*it) == node )
        {
            osg_lod_nodes_.erase(it);
            return;
        }
    }
}


//------------------------------------------------------------------------------
void LodUpdater::updateNode(OsgNodeWrapper * node)
{
    assert(node->getLodDists().size()==NUM_LOD_LVLS);
    
    Vector vp = s_scene_manager.getCamera().getPos() - node->getPosition();
    
    float dist = std::max(abs(vp.x_), abs(vp.z_)) - node->getRadius();

    unsigned new_level = NUM_LOD_LVLS;
    for (unsigned l=0; l<NUM_LOD_LVLS; ++l)
    {
        float d = /*node->getRadius() * */node->getLodDists()[l] * *lod_scale_;
        float f = l > node->getLodLevel() ? 1.0f/HYSTERESIS_FACTOR : HYSTERESIS_FACTOR;
        
        if (dist < d*f)
        {
            new_level = l;
            break;
        }
    }

    node->setLodLevel(new_level);
}
