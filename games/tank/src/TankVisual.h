
#ifndef TANK_TANK_VISUAL_INCLUDED
#define TANK_TANK_VISUAL_INCLUDED


#include "ControllableVisual.h"


#include "Scheduler.h"

namespace physics
{
    struct CollisionInfo;
    class OdeSimulator;
    class OdeCollisionSpace;
    class OdeRayGeom;
}

namespace osgParticle
{
    class RadialShooter;
    class ConstantRateCounter;
}

class Tank;
class SoundSource;
class ParticleEffectNode;


//------------------------------------------------------------------------------
class WheelVisual
{
public:
    WheelVisual();

    void init(Tank * tank,
              osg::Node * osg_node,
              float steer_factor,
              bool handbraked);
    
    void update(const Tank * tank, float dt);
    
    osg::ref_ptr<osg::MatrixTransform> model_;

    float rot_angle_;
    float penetration_;

    Vector pos_; ///< Position of lowest wheel point relative to tank.
    
    // Cached Parameters
    float steer_factor_;
    bool handbraked_;

 protected:
    bool collisionCallback(const physics::CollisionInfo & info);

    physics::OdeRayGeom * ray_; ///< Ray has length radius.
    physics::OdeCollisionSpace * static_space_;
};


//------------------------------------------------------------------------------
class TankVisual : public ControllableVisual
{
 public:
    virtual ~TankVisual();
    VISUAL_IMPL(TankVisual);

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);


    virtual Matrix getTrackingPos(const Vector & offset);

    void enableInternalView(bool b);
    bool isInternalViewEnabled() const;

    void enableSecondaryWeaponEffect(bool b);
    void fireEffect(const std::string & sound);

    void enableBoostEffect(bool b);
    
    void playTankSoundEffect(const std::string & effect, Vector pos);
    
 protected:

    TankVisual();
    
    virtual void onModelChanged();

    void initWheels();
    
    void updateCarriedObject();
    void updateSmokeEffect();
    void updateTurret();
    void updateInvincible();
    void updateShieldEffect();
    void updateWheelDirtParticles();
    void updateMgRotation(float dt);
    
    osg::ref_ptr<osg::MatrixTransform> turret_;
    osg::ref_ptr<osg::MatrixTransform> barrel_;
    std::vector<osg::ref_ptr<osg::MatrixTransform> > mg_;
    std::vector<osg::ref_ptr<ParticleEffectNode> > wheel_dirt_;
    std::vector<WheelVisual> wheel_;

    bool mg_firing_;
    
    float mg_rot_angle_;
    float mg_rot_speed_;

    osg::ref_ptr<SoundSource> engine_sound_;

    std::vector<ParticleEffectNode*> muzzle_flash_effect_;
    
    bool internal_view_;

    float last_time_;

    bool prev_healing_;         ///< these are used for performance
    bool prev_invincible_;      ///< optimizations, to avoid calling
    bool prev_shielded_;      ///< optimizations, to avoid calling
    bool prev_smoke_effect_;    ///< methods during update traversal
    
    RegisteredFpGroup fp_group_;
};

#endif
