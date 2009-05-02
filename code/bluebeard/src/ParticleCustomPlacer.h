
#ifndef TANK_PARTICLE_CUSTOM_PLACER_INCLUDED
#define TANK_PARTICLE_CUSTOM_PLACER_INCLUDED

#include <osgParticle/ModularEmitter>
#include <osgParticle/PointPlacer>
#include <osgParticle/BoxPlacer>

using namespace osgParticle;

//------------------------------------------------------------------------------
/***
 *  This class extends the original PointPlacer with the ability to apply
 *  an initial rotation to the particles placed
 **/
class PointPlacerExtended : public PointPlacer
{
public:    
    PointPlacerExtended();
    PointPlacerExtended(const PointPlacerExtended& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
    
    META_Object(bluebeard, PointPlacerExtended);

    /// Get the range of possible values for radius.
    const rangev3 & getInitialRotationRange() const;
    
    /// Set the range of possible values for radius.
    void setInitialRotationRange(const rangev3& a);
    
    /// Set the range of possible values for radius.
    void setInitialRotationRange(const osg::Vec3 & a1, const osg::Vec3 & a2);  


    /// Place a particle. Do not call it manually.
    void place(Particle* P) const;

protected:
    virtual ~PointPlacerExtended() {}
    PointPlacerExtended& operator=(const PointPlacerExtended&) { return *this; }        
    
private:
    rangev3 initial_angle_range_;
};

//------------------------------------------------------------------------------
/***
 *  This class extends the original BoxPlacer with the ability to apply
 *  an initial rotation to the particles placed
 **/
class BoxPlacerExtended : public BoxPlacer
{
public:    
    BoxPlacerExtended();
    BoxPlacerExtended(const BoxPlacerExtended& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
    
    META_Object(bluebeard, BoxPlacerExtended);

    /// Get the range of possible values for radius.
    const rangev3 & getInitialRotationRange() const;
    
    /// Set the range of possible values for radius.
    void setInitialRotationRange(const rangev3& a);
    
    /// Set the range of possible values for radius.
    void setInitialRotationRange(const osg::Vec3 & a1, const osg::Vec3 & a2);  


    /// Place a particle. Do not call it manually.
    void place(Particle* P) const;

protected:
    virtual ~BoxPlacerExtended() {}
    BoxPlacerExtended& operator=(const BoxPlacerExtended&) { return *this; }        
    
private:
    rangev3 initial_angle_range_;
};



//------------------------------------------------------------------------------
//// XXXXX to be reviewed on global/local effect discussion
class CircularPlacer : public PointPlacerExtended
{
public:    
    CircularPlacer(ModularEmitter * me);
    CircularPlacer(const CircularPlacer& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
    
    META_Object(bluebeard, CircularPlacer);

    /// Get the range of possible values for radius.
    const rangef& getRadiusRange() const;
    
    /// Set the range of possible values for radius.
    void setRadiusRange(const rangef& r);
    
    /// Set the range of possible values for radius.
    void setRadiusRange(float r1, float r2);  

    /// Set the axis the circular effect rotates around.
    void setRotationAxis(const osg::Vec3 & axis);  

    /// Get the axis the circular effect rotates around.
    osg::Vec3 getRotationAxis() const;  
    
    /// Set speed of circular effect
    void setRotationSpeed(float s);  

    /// Get speed of circular effect
    float getRotationSpeed() const; 

    /// Set angle offset of circular effect
    void setRotationOffsetAngle(float a);

    /// Get angle offset of circular effect
    float getRotationOffsetAngle() const;

    /// Place a particle. Do not call it manually.
    void place(Particle* P) const;

    void setTime(double t);

protected:
    CircularPlacer() {}
    virtual ~CircularPlacer() {}
    CircularPlacer& operator=(const CircularPlacer&) { return *this; }        
    
private:
    rangef radius_range_;
    float speed_;
    float offset_angle_;
    osg::Vec3 rotation_axis_;

    ModularEmitter * me_;
    double time_;
};







#endif

