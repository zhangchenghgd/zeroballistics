
#include "TankVisual.h"

#include <limits>

#include <osg/MatrixTransform>
#include <osg/Switch>
#include <osgParticle/ModularEmitter>
#include <osgParticle/ConstantRateCounter>


#include "physics/OdeSimulator.h"

#include "SceneManager.h"
#include "Tank.h"
#include "EffectManager.h"
#include "AutoRegister.h"
#include "SoundManager.h"
#include "SoundSource.h"
#include "SoundSourceCallbacks.h"
#include "Score.h"
#include "OsgNodeWrapper.h"
#include "BbmOsgConverter.h"
#include "Shadow.h"
#include "GameLogicClientCommon.h"

#include "ParticleCustomPlacer.h"
#include "Water.h"


#undef min
#undef max

REGISTER_CLASS(GameObjectVisual, TankVisual);

const float FACTOR_WHEEL_DIRT_PARTICLES = 15.0f;
const float FACTOR_WHEEL_DIRT_PARTICLES_SPEED = 0.4f;

const char * INTACT_GROUP_NAME    = "in";
const char * DESTROYED_GROUP_NAME = "de";

const char * EXTERNAL_GROUP_NAME  = "external";
const char * INTERNAL_GROUP_NAME  = "internal";

const char * MG_EFFECT_GROUP_NAME = "mg_effect";
const char * TRACER_EFFECT_NAME   = "tracer_red";

const char * HEAL_EFFECT_GROUP_NAME = "heal";

const char * SMOKE_GROUP_NAME = "smoke";

const char * INVINCIBLE_GROUP_NAME = "invincible";

const char * WHEEL_VISUAL_NAME = "wheel";

const unsigned NUM_INVINCIBLE_NODES = 6;

const float MG_ROT_ACC = 50.0f;
const float MG_ROT_SPEED = 25.0f;


const std::string TURRET_NODE_NAME = "turret";
const std::string BARREL_NODE_NAME = "barrel";
const std::string MG_NODE_NAME     = "mg";
const std::string WHEEL_DIRT_NODE_NAME = "wheel_dirt";
const std::string MUZZLE_FLASH_NODE_NAME = "muzzle_flash";


const std::string LEVEL_TEX_NAME = "level_light";

//------------------------------------------------------------------------------
WheelVisual::WheelVisual() :
    rot_angle_(0.0f),
    penetration_(0.0),
    pos_(Vector(0,0,0)),
    steer_factor_(0.0f),
    handbraked_(false)
{
}


//------------------------------------------------------------------------------
void WheelVisual::init(Tank * tank,
                       osg::Node * osg_node,
                       float steer_factor,
                       bool handbraked)
{
    static_space_ = tank->getProxy()->getSimulator()->getStaticSpace();

    steer_factor_ = steer_factor;
    handbraked_   = handbraked;
    // Create new ray to test for collisions.

    // First we need the wheel bounding box.
    FindMaxAabbVisitor nv;
    osg_node->accept(nv);
    const osg::BoundingBox & bb = nv.getBoundingBox();

    // Find wheel radius from BB
    float radius = 0.5f * (bb.yMax() - bb.yMin());


    // Now construct ray to test for wheel penetration
    Matrix offset(true);
    offset.loadCanonicalRotation(-PI*0.5f, 0);
    offset.getTranslation() = vecOsg2Gl(s_scene_manager.getWorldCoords(osg_node).getTrans());
    // Offset to center of wheel (object center is at inner rim)
    offset.getTranslation().x_ += sign(offset.getTranslation().x_)*(bb.xMax() - bb.xMin())*0.5f;

    ray_ = new physics::OdeRayGeom(radius);
    ray_->setName("wheelvisual " + osg_node->getName());
    ray_->setSensor(true); // or else collision category will be changed...
    ray_->setOffset(offset);
    ray_->setSpace(NULL);
    ray_->setCollisionCallback(physics::CollisionCallback(this, &WheelVisual::collisionCallback));
        
    tank->getProxy()->addGeom(ray_);
    ray_->setCategory(CCCC_WHEEL_RAY);


    pos_ = offset.getTranslation();
    pos_.y_ -= ray_->getLength();
        
    model_ = s_scene_manager.insertLocalTransformNode(osg_node);
}


//------------------------------------------------------------------------------
bool WheelVisual::collisionCallback(const physics::CollisionInfo & info)
{
    if (info.type_ == physics::CT_STOP) return false;
    if (info.other_geom_->getName() == WATER_GEOM_NAME)  return false;
    
    penetration_ = std::max(penetration_, ray_->getLength() - info.penetration_);
    
    return false;
}

//------------------------------------------------------------------------------
void WheelVisual::update(const Tank * tank, float dt)
{
    penetration_ = 0.0f;
    static_space_->collide(ray_, physics::CollisionCallback(this, &WheelVisual::collisionCallback));
    
    // Do terrain<->wheel collision
    physics::CollisionInfo info;
    info.penetration_ = penetration_;
    Tank::getTerrainData()->collideRay(info,
                                       tank->getTransform(true).transformPoint(pos_),
                                       tank->getTransform(true).getY(),
                                       0.0f,
                                       false);
    
    // Limit wheel penetration to the wheel radius, or wheels might go
    // anywhere...
    info.penetration_ = std::min(info.penetration_, ray_->getLength());
    
    // set wheel model position from penetration
    float dir_vel = -tank->getTarget()->getLocalLinearVel().z_;

    bool handbraked = tank->getPlayerInput().action2_ && handbraked_;
    if (!handbraked && !tank->isBraking() && info.penetration_ != 0.0f)
    {
        rot_angle_ = normalizeAngle(rot_angle_ + dt*dir_vel / ray_->getLength());
    }
    
    Matrix rot(true), rot_steer(true);
    rot.      loadCanonicalRotation( rot_angle_,   0);
    rot_steer.loadCanonicalRotation(tank->getProxySteerAngle()*steer_factor_, 1);
    rot_steer.getTranslation().y_ = info.penetration_;
        
    model_->setMatrix(matGl2Osg(rot_steer * rot));

}


//------------------------------------------------------------------------------
TankVisual::~TankVisual()
{
}

//------------------------------------------------------------------------------
void TankVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{   
    ControllableVisual::operator()(node, nv);

    float cur_time = nv->getFrameStamp()->getSimulationTime();
    float dt = cur_time - last_time_;
    last_time_ = cur_time;
    
    Tank * tank = (Tank*)object_;
    if (tank->getTarget()->isSleeping()) return;


    updateCarriedObject();
    updateSmokeEffect();
    updateTurret();

    for (unsigned w=0; w<wheel_.size(); ++w)
    {
        wheel_[w].update(tank, dt);
    }

    updateInvincible();
    updateWheelDirtParticles();
    updateMgRotation(dt);

    if (prev_healing_ != tank->isGainingHealth())
    {
        prev_healing_ = tank->isGainingHealth();
        EnableGroupVisitor g(HEAL_EFFECT_GROUP_NAME, prev_healing_);
        osg_wrapper_->getOsgNode()->accept(g);
    }
    
}


//------------------------------------------------------------------------------
/**
 *  Returns the position the tracking camera should be in.
 */
Matrix TankVisual::getTrackingPos(const Vector & offset)
{
    updateTransform();
    updateTurret();

    Matrix trans = matOsg2Gl(s_scene_manager.getWorldCoords(barrel_.get()) );
    
    Matrix o(true);
    o.getTranslation() = offset;

    return trans * o;
}


//------------------------------------------------------------------------------
void TankVisual::enableInternalView(bool b)
{    
    EnableGroupVisitor v1(EXTERNAL_GROUP_NAME, !b);
    EnableGroupVisitor v2(INTERNAL_GROUP_NAME, b);

    osg_wrapper_->getOsgNode()->accept(v1);
    osg_wrapper_->getOsgNode()->accept(v2);

    RigidBody * tank = (RigidBody*)object_;
    assert(tank);

    // tanks engine sound effect
    if(!engine_sound_.get())
    {
        engine_sound_ = s_soundmanager.playLoopingEffect(s_params.get<std::string>("sfx.engine"),
                                                     tank);
        engine_sound_->addUpdateCallback(new SoundSourcePositionAndPitchUpdater(tank));
    }

    if(b) // local engine sound needs no 3D attenuation
    {
        engine_sound_->setRolloffFactor(0.0f);
    }
    else
    {
        engine_sound_->setRolloffFactor(DEFAULT_ROLLOF_FACTOR);    
    }


    // disable shadow if internal view
    if (s_scene_manager.getShadow() &&
        b != internal_view_)
    {
        if (b)
        {
            s_scene_manager.getShadow()->removeBlocker(osg_wrapper_->getOsgNode());
        } else
        {
            s_scene_manager.getShadow()->addBlocker(osg_wrapper_->getOsgNode());
        }
    }


    internal_view_ = b;
}

//------------------------------------------------------------------------------
bool TankVisual::isInternalViewEnabled() const
{
    return internal_view_;
}




//------------------------------------------------------------------------------
/**
 *  stage 0 - create destroyed model, create explosion
 *  stage 1 - debris explosion
 */
void TankVisual::destroy(unsigned stage)
{
    assert(stage < 2);
    assert(dynamic_cast<Tank*>(object_));

           
    Tank * tank = (Tank*)object_;    

    // no healing effect if tank is destroyed...
    EnableGroupVisitor g(HEAL_EFFECT_GROUP_NAME, false);
    osg_wrapper_->getOsgNode()->accept(g);    
    
    if (stage == 0)
    {
        setLabelText("");

        // hide intact model, show destroyed one
        EnableGroupVisitor v1(INTACT_GROUP_NAME, false);
        EnableGroupVisitor v2(DESTROYED_GROUP_NAME, true);
        osg_wrapper_->getOsgNode()->accept(v1);
        osg_wrapper_->getOsgNode()->accept(v2);
        
        enableInternalView(false);
        
        
        // Stop engine sound
        engine_sound_->setLooping(false);
        engine_sound_->setGain(0.0f); // neccessary so sound stops immediately and not only after finishing current sample
        engine_sound_ = NULL;

        s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.explosion1"),
                                        tank->getPosition());
        
    } else // stage 1
    {
        // Tank will be deleted immediately, so add effect to osg root node
        s_scene_manager.getRootNode()->addChild(s_effect_manager.createEffect("tank_wreck_explosion",
                                                                                tank->getPosition()));
        
        s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.explosion2"),
                                        tank->getPosition());
    }
}

//------------------------------------------------------------------------------
/**
 *  Play sound and create muzzle flash
 */
void TankVisual::fireEffect(const std::string & sound)
{
    Vector sound_pos(0.0f, 0.0f, 0.0f);
    Tank * tank = (Tank*)object_;
    
    // Show muzzle flash only when tank is being watched from outside
    if (!internal_view_)
    {
        Matrix muzzle_pos;
        tank->getMuzzleTransform(&muzzle_pos);

        sound_pos = muzzle_pos.getTranslation();        
        
        for (unsigned i=0; i<muzzle_flash_effect_.size(); ++i)
        {
            muzzle_flash_effect_[i]->fire();
        }
    }
    
    playTankSoundEffect(sound, sound_pos);
}


//------------------------------------------------------------------------------
void TankVisual::enableMgEffect(bool enable)
{
    mg_firing_ = enable;
    
    EnableGroupVisitor v(MG_EFFECT_GROUP_NAME, enable);
    osg_wrapper_->getOsgNode()->accept(v);    
}

//------------------------------------------------------------------------------
void TankVisual::setMgFireDistance(float d)
{
    for (unsigned i=0; i < tracer_effect_.size(); ++i)
    {
        for (unsigned eff=0; eff<tracer_effect_[i]->getNumEffects(); ++eff)
        {
            osgParticle::ModularEmitter * emitter = tracer_effect_[i]->getEffect(eff).emitter_.get();

            osgParticle::RadialShooter * shooter = dynamic_cast<osgParticle::RadialShooter*>(emitter->getShooter());
            assert(shooter);
            
            float speed = shooter->getInitialSpeedRange().mid();

            emitter->getParticleSystem()->getDefaultParticleTemplate().setLifeTime((d-1)/speed);
        }
    }
}


//------------------------------------------------------------------------------
void TankVisual::setPlayerLevel(unsigned level)
{
    // groups
    EnableGroupVisitor v1("level" + toString(player_level_), false);
    EnableGroupVisitor v2("level" + toString(level        ), true );

    osg_wrapper_->getOsgNode()->accept(v1);
    osg_wrapper_->getOsgNode()->accept(v2);

    player_level_ = level;
}



//------------------------------------------------------------------------------
/**
 *  Plays sound effects such as cannon fire, mine dispense which must
 *  either be played at the listener's position if in internal view,
 *  or at their 3d position.
 */
void TankVisual::playTankSoundEffect(const std::string & effect, Vector pos)
{
    // Show muzzle flash only when tank is being watched from outside
    if (internal_view_)
    {
        pos = s_soundmanager.getListenerInfo().position_;
    }
    
    SoundSource * src = s_soundmanager.playSimpleEffect(effect, pos);

    // This is neccessary to avoid that e.g the firing sound trails
    // behind when moving.
    if (internal_view_)
    {
        src->addUpdateCallback(new SoundSourceListenerPositionUpdater());
    }
}


//------------------------------------------------------------------------------
TankVisual::TankVisual() :
    mg_firing_(false),
    mg_rot_angle_(0.0f),
    mg_rot_speed_(0.0f),
    internal_view_(false),
    last_time_(0.0f),
    prev_healing_(false),
    prev_invincible_(false),
    prev_smoke_effect_(true), ///< created in code, thus is enabled at first...
    player_level_(0)
{
}


//------------------------------------------------------------------------------
void TankVisual::onModelChanged()
{
    fp_group_.deregisterAllOfType(ObserverFp(NULL, NULL, 0));
    
    ControllableVisual::onModelChanged();


    std::vector<osg::Node*> turret_v = s_scene_manager.findNode(TURRET_NODE_NAME, osg_wrapper_->getOsgNode());
    std::vector<osg::Node*> barrel_v = s_scene_manager.findNode(BARREL_NODE_NAME, osg_wrapper_->getOsgNode());
    std::vector<osg::Node*> mg_v     = s_scene_manager.findNode(MG_NODE_NAME,     osg_wrapper_->getOsgNode());

    if (turret_v.size() != 1 ||
        barrel_v.size() != 1)
    {
        throw Exception("Tank model contains zero or more than one turret and/or barrel bbm nodes");
    }
    
    // insert local transform nodes for turret and barrel (and mg i.a.)
    turret_ = s_scene_manager.insertLocalTransformNode(turret_v[0]);
    barrel_ = s_scene_manager.insertLocalTransformNode(barrel_v[0]);
    for (unsigned m=0; m<mg_v.size(); ++m)
    {
        mg_.push_back(s_scene_manager.insertLocalTransformNode(mg_v[m]));
    }


    // make lvl 0 visible
    EnableGroupVisitor v0("level0", true);
    osg_wrapper_->getOsgNode()->accept(v0);

    // Show intact model
    EnableGroupVisitor v(INTACT_GROUP_NAME, true);
    osg_wrapper_->getOsgNode()->accept(v);

    // default to external view
    enableInternalView(false);

    initWheels();

    // create smoke effect
    s_effect_manager.createEffectWithGroup("tank_damaged_smoke",
                                           SMOKE_GROUP_NAME,
                                           osg_wrapper_->getOsgNode());



    // invincible effect
    CircularPlacer * cp = NULL;
    for(unsigned n=0; n < NUM_INVINCIBLE_NODES; n++)
    {
        EffectNode * inv_effect = s_effect_manager.createEffectWithGroup("tank_invincible",
                                                                         INVINCIBLE_GROUP_NAME,
                                                                         osg_wrapper_->getOsgNode());
                        
        cp =  dynamic_cast<CircularPlacer*>(inv_effect->getEffect(0).emitter_->getPlacer());
        assert(cp);

        cp->setRotationOffsetAngle((2*PI/NUM_INVINCIBLE_NODES)*n);
    }


    
    // retrieve tracer effect modeled in blender
    std::vector<osg::Node*> tracer_v = s_scene_manager.findNode(TRACER_EFFECT_NAME, osg_wrapper_->getOsgNode());
    for (unsigned i=0; i<tracer_v.size(); ++i)
    {
        EffectNode * n = dynamic_cast<EffectNode*>(tracer_v[i]);
        assert(n);
        tracer_effect_.push_back(n);
    }


    // retrieve muzzle flash effect modeled in blender
    std::vector<osg::Node*> muzzle_v = s_scene_manager.findNode(MUZZLE_FLASH_NODE_NAME, osg_wrapper_->getOsgNode());
    for (unsigned i=0; i<muzzle_v.size(); ++i)
    {
        EffectNode * n = dynamic_cast<EffectNode*>(muzzle_v[i]);
        assert(n);
        muzzle_flash_effect_.push_back(n);
    }

    
    // retrieve dirt effect modeled in blender
    std::vector<osg::Node*> dirt_v   = s_scene_manager.findNode(WHEEL_DIRT_NODE_NAME, osg_wrapper_->getOsgNode());
    for (unsigned w=0; w<dirt_v.size(); ++w)
    {
        wheel_dirt_.push_back(dynamic_cast<EffectNode*>(dirt_v[w]));
        assert(wheel_dirt_.back().get());

        // If this is not done, emitter properties are shared
        wheel_dirt_.back()->cloneEmitters();
    }
    
}


//------------------------------------------------------------------------------
void TankVisual::initWheels()
{
    Tank * tank = (Tank*)object_;
    
    // First find all wheel OSG nodes.
    std::vector<osg::Node * > wheel_node;
    for(unsigned i=0;/* no condition */;++i)
    {
        std::ostringstream name;
        name << WHEEL_VISUAL_NAME << i+1; // wheels start with 1 in modeler
        std::vector<osg::Node*> node = s_scene_manager.findNode(name.str().c_str(), osg_wrapper_->getOsgNode());
        if (node.size() != 1) break;
        wheel_node.push_back(node[0]);
    }

    // Know we know the number of wheels, required to install
    // collision callbacks later
    wheel_.resize(wheel_node.size());
    
    std::vector<float> steer_factor = tank->getParams().get<std::vector<float> >("tank.visual_steer_factor");
    std::vector<bool>  handbraked   = tank->getParams().get<std::vector<bool>  >("tank.visual_handbraked");
    if (steer_factor.size() != handbraked.size())
    {
        throw Exception("visual_steer_factor and visual_handbraked have different size");
    }
    if (steer_factor.size() != wheel_.size())
    {
        throw Exception("Different number of wheel visuals than wheel parameters.");
    }


    for(unsigned i=0; i<wheel_.size();++i)
    {
        wheel_[i].init(tank, wheel_node[i], steer_factor[i], handbraked[i]);
    }
}



//------------------------------------------------------------------------------
void TankVisual::updateCarriedObject()
{
    Tank * tank = (Tank*)object_;
    
    if (tank->getCarriedObject())
    {
        tank->positionCarriedObject();

        // We have no control over update order, so update carried
        // object position again
        RigidBodyVisual * obj = (RigidBodyVisual*)tank->getCarriedObject()->getUserData();
        if (obj) obj->updateTransform(); // XXXX Could be zero... hacky carried object handling...
    }
}


//------------------------------------------------------------------------------
void TankVisual::updateSmokeEffect()
{
    Tank * tank = (Tank*)object_;
    
    unsigned threshold = tank->getParams().get<unsigned>("tank.hp_smoking_threshold");    

    // completely destroyed tank shouldn't have smoke (interferes with
    // fire)
    bool enable_smoke = (tank->getHitpoints() <= threshold &&
                         tank->getHitpoints() != 0         &&
                         !internal_view_);
    
    if (enable_smoke == prev_smoke_effect_) return;
    prev_smoke_effect_ = enable_smoke;

    EnableGroupVisitor g(SMOKE_GROUP_NAME, enable_smoke);
    osg_wrapper_->getOsgNode()->accept(g);
}



//------------------------------------------------------------------------------
void TankVisual::updateTurret()
{
    Tank * tank = (Tank*)object_;

    float yaw,pitch;
    tank->getProxyTurretPos(yaw, pitch);
    
    Matrix rot;
    rot.loadCanonicalRotation(-pitch, 0);
    barrel_->setMatrix(matGl2Osg(rot));


    rot.loadCanonicalRotation(yaw, 1);
    turret_->setMatrix(matGl2Osg(rot));
}

//------------------------------------------------------------------------------
void TankVisual::updateInvincible()
{
    Tank * tank = (Tank*)object_;

    if(prev_invincible_ != tank->isInvincible())
    {
        prev_invincible_ = tank->isInvincible();
        EnableGroupVisitor g(INVINCIBLE_GROUP_NAME, prev_invincible_);
        osg_wrapper_->getOsgNode()->accept(g);
    }
}

//------------------------------------------------------------------------------
void TankVisual::updateWheelDirtParticles()
{
    Tank * tank = (Tank*)object_;

    Vector velocity = tank->getTarget()->getLocalLinearVel();
    float speed = -velocity.z_;

    // no particles when driving backwards
    if(velocity.z_ > 0.0) speed = 0.0f;
    
    for (unsigned p=0; p<wheel_dirt_.size(); ++p)
    {
        ((osgParticle::ConstantRateCounter*) wheel_dirt_[p]->getEffect(0).emitter_->
         getCounter())->setNumberOfParticlesPerSecondToCreate(speed * FACTOR_WHEEL_DIRT_PARTICLES);
        ((osgParticle::RadialShooter*)wheel_dirt_[p]->getEffect(0).emitter_->
         getShooter())->setInitialSpeedRange(  speed * FACTOR_WHEEL_DIRT_PARTICLES_SPEED,
                                               speed * FACTOR_WHEEL_DIRT_PARTICLES_SPEED);
    }
}

//------------------------------------------------------------------------------
void TankVisual::updateMgRotation(float dt)
{
    if (mg_firing_)
    {
        mg_rot_speed_ += dt * MG_ROT_ACC;
    } else
    {
        mg_rot_speed_ -= dt * MG_ROT_ACC;
    }
    mg_rot_speed_ = clamp(mg_rot_speed_, 0.0f, MG_ROT_SPEED);

    
    if (mg_rot_speed_)
    {
        mg_rot_angle_ += dt * mg_rot_speed_;
    
        Matrix m (true);
        m.loadCanonicalRotation(mg_rot_angle_, 2);

        for (unsigned i=0; i<mg_.size(); ++i)
        {
            mg_[i]->setMatrix(matGl2Osg(m));
        }
    }
}
