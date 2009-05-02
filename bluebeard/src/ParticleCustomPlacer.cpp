
#include "ParticleCustomPlacer.h"

#include <osgParticle/Placer>

#include "Camera.h"
#include "SceneManager.h"


//------------------------------------------------------------------------------
/**
 *  Avoid cyclic reference headaches...
 */
class CircularPlacerUpdater : public osg::NodeCallback
{
public:
    CircularPlacerUpdater(CircularPlacer * placer) : 
        placer_(placer) {}

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv)
        {
            placer_->setTime(nv->getFrameStamp()->getReferenceTime());
        }
protected:
    CircularPlacer * placer_;
};


//------------------------------------------------------------------------------
CircularPlacer::CircularPlacer(ModularEmitter * me) :
    PointPlacerExtended(),
    radius_range_(1, 1),
    speed_(1.0),
    offset_angle_(0.0),
    rotation_axis_(0,1,0),
    me_(me),
    time_(0.0)    
{
    // take care if modular emitter already has an updatecallback set
    CircularPlacerUpdater * updater = new CircularPlacerUpdater(this);

    osg::NodeCallback * current_nc = me_->getUpdateCallback();
    if(current_nc)
        current_nc->addNestedCallback(updater);
    else
        me_->setUpdateCallback(updater);
}

//------------------------------------------------------------------------------
CircularPlacer::CircularPlacer(const CircularPlacer& copy, const osg::CopyOp& copyop) :
    PointPlacerExtended(copy, copyop), 
    radius_range_(copy.radius_range_),
    speed_(copy.speed_),
    offset_angle_(copy.offset_angle_),
    rotation_axis_(copy.rotation_axis_),
    me_(copy.me_),
    time_(copy.time_)
{
    // take care if modular emitter already has an updatecallback set
    CircularPlacerUpdater * updater = new CircularPlacerUpdater(this);

    osg::NodeCallback * current_nc = me_->getUpdateCallback();
    if(current_nc)
        current_nc->addNestedCallback(updater);
    else
        me_->setUpdateCallback(updater);
}

//------------------------------------------------------------------------------
const rangef& CircularPlacer::getRadiusRange() const
{
    return radius_range_;
}

//------------------------------------------------------------------------------
void CircularPlacer::setRadiusRange(const rangef& r)
{
    radius_range_ = r;
}

//------------------------------------------------------------------------------
void CircularPlacer::setRadiusRange(float r1, float r2)
{
    radius_range_.minimum = r1;
    radius_range_.maximum = r2;
}

//------------------------------------------------------------------------------
void CircularPlacer::setRotationAxis(const osg::Vec3 & axis)
{
    rotation_axis_ = axis;
}

//------------------------------------------------------------------------------
osg::Vec3 CircularPlacer::getRotationAxis() const  
{
    return rotation_axis_;
}

//------------------------------------------------------------------------------
void CircularPlacer::setRotationSpeed(float s)
{
    speed_ = s;
}

//------------------------------------------------------------------------------
float CircularPlacer::getRotationSpeed() const
{
    return speed_;
}

//------------------------------------------------------------------------------
void CircularPlacer::setRotationOffsetAngle(float a)
{
    offset_angle_ = a;
}

//------------------------------------------------------------------------------
float CircularPlacer::getRotationOffsetAngle() const
{
    return offset_angle_;
}

//------------------------------------------------------------------------------
void CircularPlacer::place(Particle* P) const
{
    float radius = radius_range_.get_random_sqrtf();

    osg::Vec3f rotation_vec = rotation_axis_ ^ osg::Vec3f(rotation_axis_.x(), rotation_axis_.z(), rotation_axis_.y());
    
    osg::Quat q;
    q.makeRotate((speed_ * time_) + offset_angle_, rotation_axis_);

    // rotates rotation_vec around quaternion q
    osg::Vec3f final_pos = getCenter() + q * (rotation_vec * radius);

    P->setPosition(final_pos);
    
}

//------------------------------------------------------------------------------
void CircularPlacer::setTime(double t)
{ 
    time_ = t; 
}




//------------------------------------------------------------------------------
/***
 *  This class extends the original CenteredPlacer with the ability to apply
 *  an initial rotation to the particles placed
 **/
PointPlacerExtended::PointPlacerExtended() :
    PointPlacer(),
    initial_angle_range_(osg::Vec3(0,0,0), osg::Vec3(0,0,0))
{
}

//------------------------------------------------------------------------------
PointPlacerExtended::PointPlacerExtended(const PointPlacerExtended& copy, const osg::CopyOp& copyop) :
    PointPlacer(copy, copyop), 
    initial_angle_range_(copy.initial_angle_range_)
{
}

//------------------------------------------------------------------------------
const rangev3 & PointPlacerExtended::getInitialRotationRange() const
{
    return initial_angle_range_;
}

//------------------------------------------------------------------------------
void PointPlacerExtended::setInitialRotationRange(const rangev3 & a)
{
    initial_angle_range_ = a;
}

//------------------------------------------------------------------------------
void PointPlacerExtended::setInitialRotationRange(const osg::Vec3 & a1, const osg::Vec3 & a2)
{
    initial_angle_range_.minimum = a1;
    initial_angle_range_.maximum = a2;
}

//------------------------------------------------------------------------------
void PointPlacerExtended::place(Particle* P) const
{
    PointPlacer::place(P);

    osg::Vec3 angle = initial_angle_range_.get_random();

    P->setAngle(angle);
    
}


//------------------------------------------------------------------------------
/***
 *  This class extends the original BoxPlacer with the ability to apply
 *  an initial rotation to the particles placed
 **/
BoxPlacerExtended::BoxPlacerExtended() :
    BoxPlacer(),
    initial_angle_range_(osg::Vec3(0,0,0), osg::Vec3(0,0,0))
{
}

//------------------------------------------------------------------------------
BoxPlacerExtended::BoxPlacerExtended(const BoxPlacerExtended& copy, const osg::CopyOp& copyop) :
    BoxPlacer(copy, copyop), 
    initial_angle_range_(copy.initial_angle_range_)
{
}

//------------------------------------------------------------------------------
const rangev3 & BoxPlacerExtended::getInitialRotationRange() const
{
    return initial_angle_range_;
}

//------------------------------------------------------------------------------
void BoxPlacerExtended::setInitialRotationRange(const rangev3 & a)
{
    initial_angle_range_ = a;
}

//------------------------------------------------------------------------------
void BoxPlacerExtended::setInitialRotationRange(const osg::Vec3 & a1, const osg::Vec3 & a2)
{
    initial_angle_range_.minimum = a1;
    initial_angle_range_.maximum = a2;
}

//------------------------------------------------------------------------------
void BoxPlacerExtended::place(Particle* P) const
{
    BoxPlacer::place(P);

    osg::Vec3 angle = initial_angle_range_.get_random();

    P->setAngle(angle);
    
}
