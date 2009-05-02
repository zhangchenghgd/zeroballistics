
#include "ParticleEffect.h"

#include <osgParticle/Particle>
#include <osgParticle/ParticleSystem>
#include <osgParticle/ConnectedParticleSystem>
#include <osgParticle/ParticleProcessor>
#include <osgParticle/ParticleSystemUpdater>
#include <osgParticle/RadialShooter>
#include <osgParticle/AccelOperator>
#include <osgParticle/FluidFrictionOperator>
#include <osgParticle/RandomRateCounter>
#include <osgParticle/ConstantRateCounter>
#include <osgParticle/BoxPlacer>
#include <osgParticle/SectorPlacer>

#include <osg/Vec3>
#include <osg/Node>
#include <osg/TexEnv>
#include <osg/Material>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Depth>

#include <tinyxml.h>


#include "ParticleCustomOperator.h"
#include "ParticleCustomPlacer.h"
#include "ParticleCustomShooter.h"
#include "SceneManager.h"
#include "TextureManager.h"

#include "TinyXmlUtils.h"
#include "Paths.h"


const std::string FILENAME_EXTENSION = ".xml";
const float SHOOTER_OFFSET = 90.0f; ///< add this to shooter angles so that 0,0 shoots straight forward (y dir)


//------------------------------------------------------------------------------
ParticleEffect::ParticleEffect(const std::string & filename)
{
    
    pe_data_.filename_ = filename + FILENAME_EXTENSION;

    parseParticleEffect();
}

//------------------------------------------------------------------------------
ParticleEffect::~ParticleEffect()
{
}

//------------------------------------------------------------------------------
/***
 * This method creates a new particle effect out of the parsed blueprint
 **/
ParticleEffectContainer ParticleEffect::createParticleEffect()
{

    ////----------------------------------- PARTICLE SYSTEM part -----------------------------------

    osg::StateSet * stateset = new osg::StateSet;
    osg::ref_ptr<osgParticle::ParticleSystem> particle_system;

    if (pe_data_.particle_.connected_)
    {
        osgParticle::ConnectedParticleSystem * sys = new osgParticle::ConnectedParticleSystem;
        sys->setMaxNumberOfParticlesToSkip(0);
        particle_system = sys;


        // Need to disable culling / z-buffer writing manually...
        stateset->setAttributeAndModes(new osg::CullFace(osg::CullFace::BACK),
                                       osg::StateAttribute::OFF);
        osg::Depth * d = new osg::Depth;
        d->setWriteMask(0);
        stateset->setAttribute(d);
    } else
    {
        particle_system = new osgParticle::ParticleSystem;
    }
    particle_system->setName(pe_data_.filename_);

    // used to set the attributes 'texture', 'emmisive' and 'lighting'

    /// texture
    unsigned tex_unit = 0;
    Texture * tex_obj = s_texturemanager.getResource(pe_data_.particle_.tex_name_);
    osg::Texture2D * texture = tex_obj->getOsgTexture();

    texture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
    texture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
    texture->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::MIRROR);
    texture->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::MIRROR);           
    stateset->setTextureAttributeAndModes(tex_unit, texture, osg::StateAttribute::ON);

    osg::TexEnv *texenv = new osg::TexEnv;
    texenv->setMode(osg::TexEnv::MODULATE);
    stateset->setTextureAttribute(tex_unit, texenv);
    

    /// lighting
    stateset->setMode(GL_LIGHTING, pe_data_.particle_.lighting_ ? osg::StateAttribute::ON: osg::StateAttribute::OFF);
    stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    osg::Material *material = new osg::Material;
    material->setSpecular(osg::Material::FRONT, osg::Vec4(0, 0, 0, 1));
    material->setEmission(osg::Material::FRONT, osg::Vec4(0, 0, 0, 1));
    material->setColorMode(pe_data_.particle_.lighting_ ? osg::Material::AMBIENT_AND_DIFFUSE : osg::Material::OFF);
    stateset->setAttributeAndModes(material, osg::StateAttribute::ON);

    /// emissive -> BlendFunc
    osg::BlendFunc *blend = new osg::BlendFunc;
    if (pe_data_.particle_.emissive_) 
    {    
        blend->setFunction(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE);
    } else 
    {
        blend->setFunction(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA);
    }
    stateset->setAttributeAndModes(blend, osg::StateAttribute::ON);
    
    particle_system->setStateSet(stateset); // set stateset, formerly setDefaultParticleAttributes

    particle_system->setFreezeOnCull(false);
    
    // Create a particle to be used by our particle system and define a few
    // of its properties
    osgParticle::Particle particle;
    particle.setSizeRange(pe_data_.particle_.size_range_);    // meters
    particle.setColorRange(pe_data_.particle_.color_range_);
    particle.setAlphaRange(pe_data_.particle_.alpha_range_);
    particle.setLifeTime(pe_data_.particle_.lifetime_);       // seconds
    particle.setMass(pe_data_.particle_.mass_);               // in kilograms
    particle_system->setDefaultParticleTemplate(particle);


    ////----------------------------------- EMITTER part -----------------------------------

    // Create a modular emitter (this contains default counter, placer and shooter.)
    osg::ref_ptr<osgParticle::ModularEmitter> modular_emitter = new osgParticle::ModularEmitter;
    modular_emitter->setName(pe_data_.filename_);
    modular_emitter->setParticleSystem(particle_system.get());

    // set movement compensation
    modular_emitter->setNumParticlesToCreateMovementCompensationRatio(pe_data_.emitter_.movement_compenstation_ratio_);

    if(pe_data_.emitter_.ps_lifetime_ == 0)
    {
        modular_emitter->setEndless(true);
    }
    else
    {
        modular_emitter->setEndless(false);
        modular_emitter->setLifeTime(pe_data_.emitter_.ps_lifetime_);
    }

    // Counter
    if(pe_data_.emitter_.counter_.rate_range_.minimum == 0.0f && pe_data_.emitter_.counter_.rate_range_.maximum == 0.0f)
    {
        osg::ref_ptr<osgParticle::ConstantRateCounter> particle_rate = new osgParticle::ConstantRateCounter;
        particle_rate->setNumberOfParticlesPerSecondToCreate(pe_data_.emitter_.counter_.particles_per_second_);
        modular_emitter->setCounter(particle_rate.get());
    }
    else
    {
        osg::ref_ptr<osgParticle::RandomRateCounter> particle_rate = new osgParticle::RandomRateCounter;
        particle_rate->setRateRange(pe_data_.emitter_.counter_.rate_range_); 
        modular_emitter->setCounter(particle_rate.get());
    }
    


    // Placer
    if(pe_data_.emitter_.placer_.type_ == PPT_CENTERED)
    {
        osg::ref_ptr<PointPlacerExtended> particle_placer = new PointPlacerExtended();
        particle_placer->setCenter(pe_data_.emitter_.placer_.center_); 
        particle_placer->setInitialRotationRange(pe_data_.emitter_.placer_.inital_rotation_range_);
        modular_emitter->setPlacer(particle_placer.get());        
    } 
    else if(pe_data_.emitter_.placer_.type_ == PPT_BOXED)
    {
        osg::ref_ptr<BoxPlacerExtended> particle_placer = new BoxPlacerExtended();
        particle_placer->setCenter(pe_data_.emitter_.placer_.center_);
        particle_placer->setXRange(pe_data_.emitter_.placer_.range_x_);
        particle_placer->setYRange(pe_data_.emitter_.placer_.range_y_);
        particle_placer->setZRange(pe_data_.emitter_.placer_.range_z_);
        particle_placer->setInitialRotationRange(pe_data_.emitter_.placer_.inital_rotation_range_);
        modular_emitter->setPlacer(particle_placer.get());
    }
    else if(pe_data_.emitter_.placer_.type_ == PPT_CIRCULAR)
    {
        osg::ref_ptr<CircularPlacer> particle_placer = new CircularPlacer(modular_emitter.get());
        particle_placer->setCenter(pe_data_.emitter_.placer_.center_); 
        particle_placer->setInitialRotationRange(pe_data_.emitter_.placer_.inital_rotation_range_);
        particle_placer->setRadiusRange(pe_data_.emitter_.placer_.circular_radius_);
        particle_placer->setRotationAxis(pe_data_.emitter_.placer_.rotation_axis_);
        particle_placer->setRotationSpeed(pe_data_.emitter_.placer_.rotation_speed_);
        particle_placer->setRotationOffsetAngle(pe_data_.emitter_.placer_.rotation_offset_angle_);
        modular_emitter->setPlacer(particle_placer.get());  
    }

    // Shooter
    osg::ref_ptr<osgParticle::RadialShooter> particle_shooter;
    if (pe_data_.emitter_.shooter_.velocity_scale_ != 0.0f)
    {
        VelocityScaleShooter * vss = new VelocityScaleShooter(pe_data_.emitter_.shooter_.velocity_scale_);
        assert(modular_emitter->getCullCallback() == NULL);
        modular_emitter->setCullCallback(new VelocityScaleShooterCallback(vss));
        
        particle_shooter = vss;
    } else
    {
        particle_shooter = new osgParticle::RadialShooter();
    }
    modular_emitter->setShooter(particle_shooter.get());
    
    particle_shooter->setThetaRange(osg::DegreesToRadians(pe_data_.emitter_.shooter_.theta_range_.minimum + SHOOTER_OFFSET),
                                    osg::DegreesToRadians(pe_data_.emitter_.shooter_.theta_range_.maximum + SHOOTER_OFFSET)); 
    particle_shooter->setPhiRange(osg::DegreesToRadians(pe_data_.emitter_.shooter_.phi_range_.minimum + SHOOTER_OFFSET),
                                  osg::DegreesToRadians(pe_data_.emitter_.shooter_.phi_range_.maximum + SHOOTER_OFFSET)); 
    particle_shooter->setInitialSpeedRange(pe_data_.emitter_.shooter_.inital_speed_range_); // meters/second   
    particle_shooter->setInitialRotationalSpeedRange(pe_data_.emitter_.shooter_.rotational_speed_range_);
    
    

    // Create a modular program and attach it to our particle system
    osg::ref_ptr<osgParticle::ModularProgram> modular_program_ = new osgParticle::ModularProgram;
    modular_program_->setName(pe_data_.filename_);
    modular_program_->setParticleSystem(particle_system.get());
    modular_program_->setReferenceFrame(osgParticle::ParticleProcessor::ABSOLUTE_RF);

    // Create an operator that simulates acceleration (i.e.gravity)
    osg::ref_ptr<osgParticle::AccelOperator> accel  = new osgParticle::AccelOperator;
    accel->setAcceleration(pe_data_.program_.acceleration_); 
    modular_program_->addOperator(accel.get());

   
    if(pe_data_.program_.align_with_velocity_vector_)
    {
        modular_program_->addOperator(new AlignWithVelocityVectorOperator());
    }

    if(pe_data_.program_.local_effect_operator_)
    {
        modular_program_->addOperator(new LocalEffectOperator());
    }
    

    if(pe_data_.program_.use_air_friction_operator_)
    {
        // Add an operator to our program to calculate the friction of air.
        osg::ref_ptr<osgParticle::FluidFrictionOperator> air_friction = new osgParticle::FluidFrictionOperator;
        air_friction->setFluidToAir();
        modular_program_->addOperator(air_friction.get());
    }

    return ParticleEffectContainer(modular_emitter.get(), modular_program_.get());
}


//------------------------------------------------------------------------------
void ParticleEffect::parseParticleEffect()
{

    using namespace tinyxml_utils;

    TiXmlDocument xml_doc;
    TiXmlHandle root_handle = getRootHandle(PARTICLE_EFFECT_PATH + pe_data_.filename_, xml_doc);

    if (std::string("ParticleEffect") != root_handle.ToElement()->Value())
    {
        Exception e;
        e << (PARTICLE_EFFECT_PATH + pe_data_.filename_) << " is not a valid particle effect xml file.\n";
        throw e;
    }

    // Particle
    TiXmlNode * particle_node = root_handle.FirstChildElement("Particle").ToElement();
    if (particle_node)
    {
        TiXmlNode * texture_node = particle_node->FirstChild("Texture");
        if(texture_node)
        {
            pe_data_.particle_.tex_name_ = PARTICLE_TEXTURE_PATH + getAttributeString(texture_node, "value");
        }

        TiXmlNode * emissive_node = particle_node->FirstChild("Emissive");
        if(emissive_node)
        {
            pe_data_.particle_.emissive_ = getAttributeBool(emissive_node, "value");
        }

        TiXmlNode * lighting_node = particle_node->FirstChild("Lighting");
        if(lighting_node)
        {
            pe_data_.particle_.lighting_ = getAttributeBool(lighting_node, "value");
        }

        TiXmlNode * cont_node = particle_node->FirstChild("Connected");
        if(cont_node)
        {
            pe_data_.particle_.connected_ = true;
        }

        TiXmlNode * size_range_node = particle_node->FirstChild("SizeRange");
        if(size_range_node)
        {
            pe_data_.particle_.size_range_.minimum = getAttributeFloat(size_range_node, "min");
            pe_data_.particle_.size_range_.maximum = getAttributeFloat(size_range_node, "max");
        }

        TiXmlNode * color_range_node = particle_node->FirstChild("ColorRange");
        if(color_range_node)
        {
            pe_data_.particle_.color_range_.minimum.r() = getAttributeFloat(color_range_node, "min_r");
            pe_data_.particle_.color_range_.minimum.g() = getAttributeFloat(color_range_node, "min_g");
            pe_data_.particle_.color_range_.minimum.b() = getAttributeFloat(color_range_node, "min_b");
            pe_data_.particle_.color_range_.minimum.a() = getAttributeFloat(color_range_node, "min_a");
                                                 
            pe_data_.particle_.color_range_.maximum.r() = getAttributeFloat(color_range_node, "max_r");
            pe_data_.particle_.color_range_.maximum.g() = getAttributeFloat(color_range_node, "max_g");
            pe_data_.particle_.color_range_.maximum.b() = getAttributeFloat(color_range_node, "max_b");
            pe_data_.particle_.color_range_.maximum.a() = getAttributeFloat(color_range_node, "max_a");      
        }

        TiXmlNode * alpha_range_node = particle_node->FirstChild("AlphaRange");
        if(alpha_range_node)
        {
            pe_data_.particle_.alpha_range_.minimum = getAttributeFloat(alpha_range_node, "min");
            pe_data_.particle_.alpha_range_.maximum = getAttributeFloat(alpha_range_node, "max");
        }

        TiXmlNode * lifetime_node = particle_node->FirstChild("LifeTime");
        if(lifetime_node)
        {
            pe_data_.particle_.lifetime_ = getAttributeFloat(lifetime_node, "value");
        }

        TiXmlNode * mass_node = particle_node->FirstChild("Mass");
        if(mass_node)
        {
            pe_data_.particle_.mass_ = getAttributeFloat(mass_node, "value");
        }
    }

    // Emitter
    TiXmlNode * emitter_node = root_handle.FirstChildElement("Emitter").ToElement();
    if (emitter_node)
    {
        TiXmlNode * emitter_lifetime_node = emitter_node->FirstChild("LifeTime");
        if(emitter_lifetime_node)
        {
            pe_data_.emitter_.ps_lifetime_ = getAttributeFloat(emitter_lifetime_node, "value");
        }

        TiXmlNode * emitter_movement_compensation_node = emitter_node->FirstChild("MovementCompensationRatio");
        if(emitter_movement_compensation_node)
        {
            pe_data_.emitter_.movement_compenstation_ratio_ = getAttributeFloat(emitter_movement_compensation_node, "value");
        }

        // Counter
        TiXmlNode * counter_node = emitter_node->FirstChild("Counter");
        if(counter_node)
        {
            TiXmlNode * random_counter_node = counter_node->FirstChild("RandomRateRange");
            if(random_counter_node)
            {
                pe_data_.emitter_.counter_.rate_range_.minimum = getAttributeFloat(random_counter_node, "min");
                pe_data_.emitter_.counter_.rate_range_.maximum = getAttributeFloat(random_counter_node, "max");
            }  

            TiXmlNode * constant_counter_node = counter_node->FirstChild("ConstantRate");
            if(constant_counter_node)
            {
                pe_data_.emitter_.counter_.particles_per_second_ = getAttributeFloat(constant_counter_node, "value");
            }  
        }

        // Placer
        TiXmlNode * placer_node = emitter_node->FirstChild("Placer");
        if(placer_node)
        {
            // Centered Placer
            TiXmlNode * centered_node = placer_node->FirstChild("CenteredPlacer");
            if(centered_node)
            {
                pe_data_.emitter_.placer_.type_ = PPT_CENTERED;

                TiXmlNode * center_node = centered_node->FirstChild("Center");
                if(center_node)
                {
                    pe_data_.emitter_.placer_.center_.x() = getAttributeFloat(center_node, "x");
                    pe_data_.emitter_.placer_.center_.y() = getAttributeFloat(center_node, "y");
                    pe_data_.emitter_.placer_.center_.z() = getAttributeFloat(center_node, "z");
                }

                TiXmlNode * init_rot_node = centered_node->FirstChild("InitalRotationRange");
                if(init_rot_node)
                {
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.x() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_x"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.x() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_x"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.y() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_y"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.y() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_y"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.z() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_z"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.z() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_z"));
                }
            }

            // Boxed Placer
            TiXmlNode * boxed_node = placer_node->FirstChild("BoxedPlacer");
            if(boxed_node)
            {
                pe_data_.emitter_.placer_.type_ = PPT_BOXED;

                TiXmlNode * center_node = boxed_node->FirstChild("Center");
                if(center_node)
                {
                    pe_data_.emitter_.placer_.center_.x() = getAttributeFloat(center_node, "x");
                    pe_data_.emitter_.placer_.center_.y() = getAttributeFloat(center_node, "y");
                    pe_data_.emitter_.placer_.center_.z() = getAttributeFloat(center_node, "z");
                }

                TiXmlNode * init_rot_node = boxed_node->FirstChild("InitalRotationRange");
                if(init_rot_node)
                {
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.x() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_x"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.x() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_x"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.y() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_y"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.y() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_y"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.z() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_z"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.z() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_z"));
                }

                TiXmlNode * range_x_node = boxed_node->FirstChild("RangeX");
                if(range_x_node)
                {
                    pe_data_.emitter_.placer_.range_x_.minimum = getAttributeFloat(range_x_node, "min");
                    pe_data_.emitter_.placer_.range_x_.maximum = getAttributeFloat(range_x_node, "max");
                }
                TiXmlNode * range_y_node = boxed_node->FirstChild("RangeY");
                if(range_y_node)
                {
                    pe_data_.emitter_.placer_.range_y_.minimum = getAttributeFloat(range_y_node, "min");
                    pe_data_.emitter_.placer_.range_y_.maximum = getAttributeFloat(range_y_node, "max");
                }
                TiXmlNode * range_z_node = boxed_node->FirstChild("RangeZ");
                if(range_z_node)
                {
                    pe_data_.emitter_.placer_.range_z_.minimum = getAttributeFloat(range_z_node, "min");
                    pe_data_.emitter_.placer_.range_z_.maximum = getAttributeFloat(range_z_node, "max");
                }
            }

            // Circulart Placer
            TiXmlNode * circular_node = placer_node->FirstChild("CircularPlacer");
            if(circular_node)
            {
                pe_data_.emitter_.placer_.type_ = PPT_CIRCULAR;

                TiXmlNode * center_node = circular_node->FirstChild("Center");
                if(center_node)
                {
                    pe_data_.emitter_.placer_.center_.x() = getAttributeFloat(center_node, "x");
                    pe_data_.emitter_.placer_.center_.y() = getAttributeFloat(center_node, "y");
                    pe_data_.emitter_.placer_.center_.z() = getAttributeFloat(center_node, "z");
                }

                TiXmlNode * init_rot_node = circular_node->FirstChild("InitalRotationRange");
                if(init_rot_node)
                {
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.x() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_x"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.x() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_x"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.y() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_y"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.y() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_y"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.minimum.z() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "min_z"));
                    pe_data_.emitter_.placer_.inital_rotation_range_.maximum.z() = osg::DegreesToRadians(getAttributeFloat(init_rot_node, "max_z"));
                }

                TiXmlNode * radius_node = circular_node->FirstChild("Radius");
                if(radius_node)
                {
                    pe_data_.emitter_.placer_.circular_radius_.minimum = getAttributeFloat(radius_node, "min");
                    pe_data_.emitter_.placer_.circular_radius_.maximum = getAttributeFloat(radius_node, "max");
                }

                TiXmlNode * rot_axis_node = circular_node->FirstChild("RotationAxis");
                if(rot_axis_node)
                {
                    pe_data_.emitter_.placer_.rotation_axis_.x() = getAttributeFloat(rot_axis_node, "x");
                    pe_data_.emitter_.placer_.rotation_axis_.y() = getAttributeFloat(rot_axis_node, "y");
                    pe_data_.emitter_.placer_.rotation_axis_.z() = getAttributeFloat(rot_axis_node, "z");
                }

                TiXmlNode * rot_speed_node = circular_node->FirstChild("RotationSpeed");
                if(rot_speed_node)
                {
                    pe_data_.emitter_.placer_.rotation_speed_ = getAttributeFloat(rot_speed_node, "value");
                }

                TiXmlNode * rot_offset_angle_node = circular_node->FirstChild("RotationOffsetAngle");
                if(rot_offset_angle_node)
                {
                    pe_data_.emitter_.placer_.rotation_offset_angle_ = osg::DegreesToRadians(getAttributeFloat(rot_offset_angle_node, "value"));
                }
            }

        }

        // Shooter
        TiXmlNode * shooter_node = emitter_node->FirstChild("Shooter");
        if(shooter_node)
        {
            TiXmlNode * init_speed_node = shooter_node->FirstChild("InitialSpeedRange");
            if(init_speed_node)
            {
                pe_data_.emitter_.shooter_.inital_speed_range_.minimum = getAttributeFloat(init_speed_node, "min");
                pe_data_.emitter_.shooter_.inital_speed_range_.maximum = getAttributeFloat(init_speed_node, "max");
            } 

            TiXmlNode * rot_speed_node = shooter_node->FirstChild("RotationalSpeedRange");
            if(rot_speed_node)
            {
                pe_data_.emitter_.shooter_.rotational_speed_range_.minimum.x() = getAttributeFloat(rot_speed_node, "min_x");
                pe_data_.emitter_.shooter_.rotational_speed_range_.maximum.x() = getAttributeFloat(rot_speed_node, "max_x");
                pe_data_.emitter_.shooter_.rotational_speed_range_.minimum.y() = getAttributeFloat(rot_speed_node, "min_y");
                pe_data_.emitter_.shooter_.rotational_speed_range_.maximum.y() = getAttributeFloat(rot_speed_node, "max_y");
                pe_data_.emitter_.shooter_.rotational_speed_range_.minimum.z() = getAttributeFloat(rot_speed_node, "min_z");
                pe_data_.emitter_.shooter_.rotational_speed_range_.maximum.z() = getAttributeFloat(rot_speed_node, "max_z");
            } 

            TiXmlNode * theta_node = shooter_node->FirstChild("ThetaRange");
            if(theta_node)
            {
                pe_data_.emitter_.shooter_.theta_range_.minimum = getAttributeFloat(theta_node, "min");
                pe_data_.emitter_.shooter_.theta_range_.maximum = getAttributeFloat(theta_node, "max");
            } 

            TiXmlNode * phi_node = shooter_node->FirstChild("PhiRange");
            if(phi_node)
            {
                pe_data_.emitter_.shooter_.phi_range_.minimum = getAttributeFloat(phi_node, "min");
                pe_data_.emitter_.shooter_.phi_range_.maximum = getAttributeFloat(phi_node, "max");
            }
            
            TiXmlNode * v_scale_node = shooter_node->FirstChild("VelocityScale");
            if(v_scale_node)
            {
                pe_data_.emitter_.shooter_.velocity_scale_ = getAttributeFloat(v_scale_node, "factor");
            } 
        }
    }

    TiXmlNode * program_node = root_handle.FirstChildElement("Program").ToElement();
    if(program_node)
    {
        TiXmlNode * accell_operator_node = program_node->FirstChild("AccelerationOperator");
        if(accell_operator_node)
        {
            pe_data_.program_.acceleration_.x() = getAttributeFloat(accell_operator_node, "x");
            pe_data_.program_.acceleration_.y() = getAttributeFloat(accell_operator_node, "y");
            pe_data_.program_.acceleration_.z() = getAttributeFloat(accell_operator_node, "z");
        }

        if(program_node->FirstChild("AlignWithVelocityVectorOperator"))
        {
            pe_data_.program_.align_with_velocity_vector_ = true;
        }

        if(program_node->FirstChild("LocalEffectOperator"))
        {
            pe_data_.program_.local_effect_operator_ = true;
        }

        TiXmlNode * air_friction_operator_node = program_node->FirstChild("AirFrictionOperator");
        if(air_friction_operator_node)
        {
            pe_data_.program_.use_air_friction_operator_ = true;
        }
    }

}

