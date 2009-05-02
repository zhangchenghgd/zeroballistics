
#ifndef TANK_PARTICLE_EFFECT_INCLUDED
#define TANK_PARTICLE_EFFECT_INCLUDED


#include <osg/Node>
#include <osg/ref_ptr>

#include <osgParticle/range>
#include <osgParticle/ModularEmitter>
#include <osgParticle/ModularProgram>


#include "UtilsOsg.h"



//------------------------------------------------------------------------------
enum PE_PLACER_TYPE
{
    PPT_UNDEFINED,
    PPT_CENTERED,
    PPT_BOXED,
    PPT_CIRCULAR
};

//------------------------------------------------------------------------------
/**
 * Struct holding Particle Effect Data parsed from XML file
 **/
struct ParticleEffectData
{
    struct Particle
    {
        std::string tex_name_;
        bool emissive_;
        bool lighting_;
        bool connected_;
        osgParticle::rangef size_range_;
        osgParticle::rangev4 color_range_;
        osgParticle::rangef alpha_range_;
        float lifetime_;
        float mass_;
    } particle_;

    struct Emitter
    {
        float ps_lifetime_;
        float movement_compenstation_ratio_;
         
        struct Counter
        {
            osgParticle::rangef rate_range_;
            float particles_per_second_;
        } counter_;

        struct Placer
        {
            PE_PLACER_TYPE type_;  //
            osg::Vec3 center_;
            osgParticle::rangev3 inital_rotation_range_;
            osgParticle::rangef range_x_;
            osgParticle::rangef range_y_;
            osgParticle::rangef range_z_;
            osgParticle::rangef circular_radius_; // circular only
            osg::Vec3 rotation_axis_; // circular only
            float rotation_speed_; // circular only
            float rotation_offset_angle_; // circular only
        } placer_;

        struct Shooter
        {
            osgParticle::rangef inital_speed_range_;
            osgParticle::rangev3 rotational_speed_range_;
            osgParticle::rangef theta_range_;
            osgParticle::rangef phi_range_;
            float velocity_scale_;
        } shooter_;
    } emitter_;

    struct Program
    {
        osg::Vec3 acceleration_;
        bool align_with_velocity_vector_;
        bool local_effect_operator_;
        bool use_air_friction_operator_;
    } program_;


    std::string filename_;  ///< The name of the particle effect file

    // init all values to zero, to avoid crappy values
    ParticleEffectData()
    {
        particle_.tex_name_ = "";
        particle_.emissive_ = false;
        particle_.lighting_ = false;
        particle_.connected_ = false;
        particle_.size_range_ = osgParticle::rangef(0,0);
        particle_.color_range_ = osgParticle::rangev4(osg::Vec4(),osg::Vec4());
        particle_.alpha_range_ = osgParticle::rangef(0,0);
        particle_.lifetime_ = 0;
        particle_.mass_ = 0;

        emitter_.ps_lifetime_ = 0;
        emitter_.movement_compenstation_ratio_ = 0.0;
        emitter_.counter_.rate_range_ = osgParticle::rangef(0,0);
        emitter_.counter_.particles_per_second_ = 0;
        emitter_.placer_.type_ = PPT_UNDEFINED;
        emitter_.placer_.center_ = osg::Vec3();
        emitter_.placer_.inital_rotation_range_ = osgParticle::rangev3(osg::Vec3(),osg::Vec3());
        emitter_.placer_.range_x_ = osgParticle::rangef(0,0);
        emitter_.placer_.range_y_ = osgParticle::rangef(0,0);
        emitter_.placer_.range_z_ = osgParticle::rangef(0,0);
        emitter_.placer_.circular_radius_ = osgParticle::rangef(1,1); // circular only
        emitter_.placer_.rotation_axis_ = osg::Vec3(0,1,0); // circular only
        emitter_.placer_.rotation_speed_ = 1.0f; // circular only
        emitter_.placer_.rotation_offset_angle_ = 0.0f; // circular only
        emitter_.shooter_.inital_speed_range_ = osgParticle::rangef(0,0);
        emitter_.shooter_.rotational_speed_range_ = osgParticle::rangev3(osg::Vec3(),osg::Vec3());
        emitter_.shooter_.theta_range_ = osgParticle::rangef(0,0);
        emitter_.shooter_.phi_range_ = osgParticle::rangef(0,0);
        emitter_.shooter_.velocity_scale_ = 0.0f;

        
        
        program_.acceleration_ = osg::Vec3();
        program_.align_with_velocity_vector_ = false;
        program_.local_effect_operator_ = false;
        program_.use_air_friction_operator_ = false;

        filename_ = "";
    }

};

//------------------------------------------------------------------------------
/**
 * Helper class holding the components of a created Particle Effect
 **/
class ParticleEffectContainer
{
public:
    ParticleEffectContainer() {}
    ParticleEffectContainer(osgParticle::ModularEmitter * em,
                            osgParticle::ModularProgram * p) :
        emitter_(em),
        program_(p) {}
    
    osg::ref_ptr<osgParticle::ModularEmitter> emitter_;
    osg::ref_ptr<osgParticle::ModularProgram> program_;
};

//------------------------------------------------------------------------------
/**
 * Class holding Particle Effects blueprints, used as prototype
 * for each generated particle effect
 *
 **/
class ParticleEffect
{
public:
    ParticleEffect(const std::string & filename);
    virtual ~ParticleEffect();

    ParticleEffectContainer createParticleEffect();
    
private:
    void parseParticleEffect();

    ParticleEffectData pe_data_;
};



#endif
