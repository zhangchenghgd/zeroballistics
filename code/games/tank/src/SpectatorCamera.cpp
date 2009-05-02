
#include "SpectatorCamera.h"

#include <limits>

#include <loki/static_check.h>


#include "ParameterManager.h"
#include "Controllable.h"
#include "ControllableVisual.h"
#include "GameLogicClientCommon.h"
#include "GameHud.h"
#include "Score.h"
#include "InputHandler.h"
#include "SoundManager.h"

#undef min
#undef max


/// Used for terrain collision detection
const float CAMERA_RADIUS = 0.18f;

const Vector TV_CAM_OFFSET(2,1,-10);
const float TV_MAX_DISTANCE_SQR = 12*12;

const Vector TRACKING_OFFSET_3RD = Vector(0,0.3,2.5);

const char* CAMERA_MODE_NAME[] = { "free",
                                   "tracking",
                                   "TV",
                                   "1st person",
                                   "3rd person" };


//------------------------------------------------------------------------------
SpectatorCamera::SpectatorCamera(GameLogicClientCommon * game_logic) :
    camera_mode_(CM_FREE),
    last_target_position_(Vector(0,0,0)),
    target_body_(NULL),
    target_player_(UNASSIGNED_SYSTEM_ADDRESS),
    game_logic_(game_logic)        
{
    LOKI_STATIC_CHECK(sizeof(CAMERA_MODE_NAME) / sizeof(const char*) == CM_LAST, update_camera_mode_names);

    using namespace input_handler;


#ifdef ENABLE_DEV_FEATURES    
    s_input_handler.registerInputCallback(
        "Free Camera",       SingleInputHandler(this, &SpectatorCamera::modeFree),     &fp_group_);
#endif
    s_input_handler.registerInputCallback(
        "Tracking Camera",   SingleInputHandler(this, &SpectatorCamera::modeTracking), &fp_group_);
    s_input_handler.registerInputCallback(
        "TV Camera",         SingleInputHandler(this, &SpectatorCamera::modeTv),       &fp_group_);
    s_input_handler.registerInputCallback(
        "1st Person Camera", SingleInputHandler(this, &SpectatorCamera::mode1St),      &fp_group_);
    s_input_handler.registerInputCallback(
        "3rd Person Camera", SingleInputHandler(this, &SpectatorCamera::mode3rd),      &fp_group_);
}

//------------------------------------------------------------------------------
SpectatorCamera::~SpectatorCamera()
{
}


//------------------------------------------------------------------------------
void SpectatorCamera::clearMotion()
{
    Entity::clearMotion();
}


//------------------------------------------------------------------------------
void SpectatorCamera::setTransform(const Matrix & trans)
{
    Entity::setTransform(trans);
}


//------------------------------------------------------------------------------
Matrix SpectatorCamera::getTransform() const
{
    if (!target_body_) return *this;
    RigidBodyVisual * visual = (RigidBodyVisual*)target_body_->getUserData();
    if (!visual) return *this;

    Matrix ret;
    switch (camera_mode_)
    {
    case CM_FREE:
    case CM_TRACKING_3RD_CONSTANT_DIR:
    case CM_TV:
        return *this;
    case CM_TRACKING_1ST:
        return visual->getTrackingPos(Vector(0,0,0));
    case CM_TRACKING_3RD:
        ret = visual->getTrackingPos(last_target_position_);
        moveOutsideTerrain(ret);
        return ret;
    default:
        assert(false);
        return *this;
    }
}


//------------------------------------------------------------------------------
void SpectatorCamera::update(float dt)
{
    if (camera_mode_ == CM_TRACKING_3RD_CONSTANT_DIR &&
        target_body_)
    {
        Vector target_pos = target_body_->getTransform(true).getTranslation();
        getTranslation() += target_pos - last_target_position_;
        last_target_position_ = target_pos;    
    }

    frameMove(dt);

    if (camera_mode_ == CM_TV &&
        target_body_)
    {
        Vector dir = target_body_->getTransform(true).getTranslation() - getTranslation();

        // Re-position TV camera if distance becomes too great
        if (dir.lengthSqr() > TV_MAX_DISTANCE_SQR)
        {
            Matrix o(true);
            if (!equalsZero(target_body_->getGlobalLinearVel().lengthSqr()))
            {
                o.loadOrientation(target_body_->getGlobalLinearVel(), Vector(0,1,0));
            } else
            {
                o = target_body_->getTransform(true);
            }
            getTranslation() = (target_body_->getPosition() +
                                o.transformVector(TV_CAM_OFFSET));

            dir = target_body_->getTransform(true).getTranslation() - getTranslation();
        }
        
        
        Matrix m(true);
        m.getTranslation() = getTranslation();
        m.loadOrientation(dir, Vector(0,1,0));
        setTransform(m);
    }

    if (camera_mode_ == CM_TRACKING_3RD) last_target_position_ += dt*v_local_;
    else moveOutsideTerrain(*this);


    // To sync free camera with 1st / 3rd person tracking cams
    if (camera_mode_ == CM_TRACKING_1ST ||
        camera_mode_ == CM_TRACKING_3RD) setTransform(getTransform());


    // XXXX why negative values???
    Vector v = -v_global_;
    v -= transformVector(v_local_);
    if (target_body_ &&
        (camera_mode_ == CM_TRACKING_3RD_CONSTANT_DIR ||
         camera_mode_ == CM_TRACKING_1ST ||
         camera_mode_ == CM_TRACKING_3RD))
    {
        v -= target_body_->getGlobalLinearVel();
    }

    s_soundmanager.setListenerVelocity(v);
}

//------------------------------------------------------------------------------
void SpectatorCamera::handleInput(const PlayerInput & input)
{
    // Handle tracking target switches
    if (input.fire1_ == IKS_JUST_PRESSED ||
        input.fire1_ == IKS_PRESSED_AND_RELEASED) changeTrackedPlayer(false);
    if (input.fire2_ == IKS_JUST_PRESSED ||
        input.fire2_ == IKS_PRESSED_AND_RELEASED) changeTrackedPlayer(true);        


    v_local_ = Vector((bool)input.right_   - (bool)input.left_,
                      0.0f,
                      (bool)input.down_    - (bool)input.up_);
    v_local_ *= s_params.get<float>("camera.move_speed");
    // speed up free cam with sec. skill
    if (input.fire4_) v_local_ *= 12.0f;    


    if (!target_body_ ||
        (camera_mode_ != CM_TV &&
         camera_mode_ != CM_TRACKING_3RD))
    {
        v_yaw_   = -input.delta_yaw_;
        v_pitch_ = -input.delta_pitch_;
    }
}



//------------------------------------------------------------------------------
void SpectatorCamera::trackPlayer(SystemAddress player_id)
{
    PlayerScore * score = game_logic_->getScore().getPlayerScore(player_id);
    Controllable * controllable = score ? score->getPlayer()->getControllable() : NULL;

    trackBody(controllable);

    target_player_ = player_id;

    if (camera_mode_ == CM_TRACKING_1ST) game_logic_->enableLocalView(true);    
}


//------------------------------------------------------------------------------
void SpectatorCamera::trackBody(RigidBody * body)
{
    if (target_body_)
    {
        game_logic_->enableLocalView(false);
        fp_group_.deregister(ObserverFp(&fp_group_,
                                        target_body_,
                                        GOE_SCHEDULED_FOR_DELETION));
    } else if (camera_mode_ == CM_TRACKING_3RD)
    {
        loadIdentity();
        getTranslation() = TRACKING_OFFSET_3RD;
    } 

    
    target_player_ = UNASSIGNED_SYSTEM_ADDRESS;
    target_body_   = body;
    
    if (target_body_)
    {
        target_body_->addObserver(
            ObserverCallbackFun0(this, &SpectatorCamera::onTrackedObjectScheduledForDeletion),
            GOE_SCHEDULED_FOR_DELETION,
            &fp_group_);
    }
}




//------------------------------------------------------------------------------
void SpectatorCamera::setMode(CAMERA_MODE new_mode, bool hud_msg)
{
    // Always switch to tank control if 1st person in own tank to
    // avoid confusion
    if (new_mode == CM_TRACKING_1ST &&
        target_player_ == game_logic_->getPuppetMaster()->getLocalPlayer()->getId())
    {
        game_logic_->setInputMode(IM_CONTROL_TANK);
    }
    
    clearMotion();

    
    if (new_mode == camera_mode_) return;
    
    // Update the free camera to start at 3rd person tracking pos
    if (target_body_ &&
        (camera_mode_ == CM_TRACKING_3RD ||
         camera_mode_ == CM_TRACKING_1ST))
    {
        ControllableVisual * visual = (ControllableVisual*)target_body_->getUserData();   
        setTransform(visual->getTrackingPos(TRACKING_OFFSET_3RD));
    }
    
    camera_mode_ = new_mode;

    if (camera_mode_ == CM_TRACKING_3RD_CONSTANT_DIR)
    {
        
        if (target_body_) last_target_position_ = target_body_->getPosition();
        else                      last_target_position_ = getTranslation();
    } else if (camera_mode_ == CM_TRACKING_3RD)
    {
        last_target_position_ = TRACKING_OFFSET_3RD;
    }
    
    game_logic_->enableLocalView(camera_mode_ == CM_TRACKING_1ST);    
    
    if (hud_msg)
    {
        game_logic_->getPuppetMaster()->getHud()->addMessage(std::string("Switched to ")    +
                                                             CAMERA_MODE_NAME[camera_mode_] +
                                                             " camera.");
    }    
}


//------------------------------------------------------------------------------
CAMERA_MODE SpectatorCamera::getMode() const
{
    return camera_mode_;
}

//------------------------------------------------------------------------------
RigidBody * SpectatorCamera::getTargetBody() const
{
    return target_body_;
}

//------------------------------------------------------------------------------
const SystemAddress & SpectatorCamera::getTargetPlayer() const
{
    return target_player_;
}


//------------------------------------------------------------------------------
/**
 *  \param next Whether to track the next or the previous player.
 */
void SpectatorCamera::changeTrackedPlayer(bool next)
{           
    std::vector<PlayerScore> player_list = game_logic_->getScore().getPlayers();
    assert(!player_list.empty());
    std::vector<PlayerScore>::const_iterator it = find(player_list.begin(),
                                                       player_list.end(),
                                                       target_player_);        

    if (it == player_list.end())
    {
        // no player tracked previously or tracked player
        // disconnected, start from beginning
        it = player_list.begin();
    } else
    {
        if (next)
        {
            if (++it == player_list.end()) it = player_list.begin();
        } else
        {
            if (it == player_list.begin()) it = player_list.end();
            --it;
        }
    }

            
    // Now traverse players until we find one that is currently playing
    std::vector<PlayerScore>::const_iterator starting_it = it;
    while (!it->getTeam())
    {
        if (next)
        {
            if (++it == player_list.end()) it = player_list.begin();
        } else
        {
            if (it == player_list.begin()) it = player_list.end();
            --it;            
        }
        // We are once around and found no suitable player
        if (it == starting_it)
        {
            it = player_list.end();
            break;
        }
    }

    if (it == player_list.end())
    {
        trackPlayer(UNASSIGNED_SYSTEM_ADDRESS);
        game_logic_->getPuppetMaster()->getHud()->addMessage("No active player to track.");
    } else
    {
        trackPlayer(it->getPlayer()->getId());
        game_logic_->getPuppetMaster()->getHud()->addMessage("Tracking " + it->getPlayer()->getName() + ".");
    }
}



//------------------------------------------------------------------------------
void SpectatorCamera::onTrackedObjectScheduledForDeletion()
{
    target_body_ = NULL;
    clearMotion();
}




//------------------------------------------------------------------------------
void SpectatorCamera::moveOutsideTerrain(Matrix & trans) const
{
    const terrain::TerrainData * td = game_logic_->getPuppetMaster()->getGameState()->getTerrainData();
    if (td)
    {
        Vector & pos = trans.getTranslation();
        float height;
        Vector n;
        td->getHeightAndNormal(pos.x_, pos.z_, height, n);
        pos.y_ = std::max(height+CAMERA_RADIUS, pos.y_);
    }
}


//------------------------------------------------------------------------------
void SpectatorCamera::modeFree()
{
    setMode(CM_FREE, true);
}


//------------------------------------------------------------------------------
void SpectatorCamera::modeTracking()
{
    setMode(CM_TRACKING_3RD_CONSTANT_DIR, true);
}


//------------------------------------------------------------------------------
void SpectatorCamera::modeTv()
{
    setMode(CM_TV, true);
}


//------------------------------------------------------------------------------
void SpectatorCamera::mode1St()
{
    setMode(CM_TRACKING_1ST, true);
}


//------------------------------------------------------------------------------
void SpectatorCamera::mode3rd()
{
    setMode(CM_TRACKING_3RD, true);
}
