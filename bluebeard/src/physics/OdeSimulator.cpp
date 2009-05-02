
#include "OdeSimulator.h"


#ifdef ENABLE_DEV_FEATURES
#include <GL/glut.h>
#endif

#include "Log.h"
#include "OdeRigidBody.h"

#include "RigidBody.h"
#include "Profiler.h"
#include "ParameterManager.h"

#undef min
#undef max

namespace physics
{

const float CENTER_COG_RENDER_RADIUS = 0.01f;
    
const float MAX_VELOCITY_COMPONENT = 50; // PPPP
const float MAX_ANG_VELOCITY_COMPONENT = 20; // PPPP
    
const float WORLD_ERP = 0.2;
const float WORLD_CFM = 1e-4;
    
const unsigned MAX_NUM_CONTACTS       = 20;    // PPPP


unsigned OdeSimulator::instance_count_ = 0;    

//------------------------------------------------------------------------------
OdeSimulator::OdeSimulator(const std::string & name) :
    name_(name),
    world_id_(dWorldCreate()),
    static_space_(new OdeCollisionSpace(name+"-static", true)),
    actor_space_ (new OdeCollisionSpace(name+"-actor",  true)),
    contact_group_id_(dJointGroupCreate(0))
{
    if (++instance_count_ == 1);// dInitODE(); ODE 0.9

    dWorldSetGravity(world_id_, 0, -s_params.get<float>("physics.gravity"), 0);

    dWorldSetERP(world_id_, WORLD_ERP);
    dWorldSetCFM(world_id_, WORLD_CFM);

    // As a default, all categories collide with each other.
    memset(category_collide_flag_, 0xff, sizeof(category_collide_flag_));


    // Enable autodisable
    dWorldSetAutoDisableFlag(world_id_, true);

    dWorldSetAutoDisableLinearThreshold  (world_id_, s_params.get<float>   ("physics.auto_disable_lin_threshold"));
    dWorldSetAutoDisableAngularThreshold (world_id_, s_params.get<float>   ("physics.auto_disable_ang_threshold"));
    dWorldSetAutoDisableTime             (world_id_, s_params.get<float>   ("physics.auto_disable_time"));
    dWorldSetAutoDisableSteps            (world_id_, s_params.get<unsigned>("physics.auto_disable_steps"));
    

    lin_dampening_          = s_params.get<float>("physics.lin_dampening");    
    ang_dampening_          = s_params.get<float>("physics.ang_dampening");    
    water_dampening_factor_ = s_params.get<float>("physics.water_dampening_factor");    
}


//------------------------------------------------------------------------------
OdeSimulator::~OdeSimulator()
{
    s_log << Log::debug('d')
          << "OdeSimulator destructor\n";
    
    if (!body_.empty())
    {
        s_log << Log::debug('d')
              << "There are still "
              << body_.size()
              << " bodies in the simulator. Cleaning up...\n";

        while (!body_.empty())
        {
            delete *body_.begin();
        }

        s_log << Log::debug('d') << "Finished cleaning up.\n";
    }


    // spaces have to be destroyed before dWorldDestroy
    delete static_space_;
    delete actor_space_;

    dJointGroupDestroy(contact_group_id_);
    dWorldDestroy(world_id_);

    if (--instance_count_ == 0) dCloseODE();
}

//------------------------------------------------------------------------------
void OdeSimulator::frameMove(float dt)
{
    PROFILE(OdeSimulator::frameMove);
    
    handleContinousGeoms();


    actor_space_->collide(static_space_, false);
    actor_space_->collide();

    handleBodyVelocities();

    {
        PROFILE(step);
        dWorldQuickStep( world_id_, dt );
//        dWorldStep( world_id_, dt );
    }

    dJointGroupEmpty(contact_group_id_);
}



//------------------------------------------------------------------------------
/**
 *  For debug purposes only: render all shapes in this simulator.
 */
void OdeSimulator::renderGeoms() const
{
#ifndef DEDICATED_SERVER    
#ifdef ENABLE_DEV_FEATURES
    
    const static GLfloat category_color[][4] = { {1.0, 0.0, 1.0, 1.0 },   // CCC_STATIC     : Magenta
                                                 {0.0, 1.0, 1.0, 1.0 },   // CCC_PROXY      : Cyan
                                                 {1.0, 1.0, 0.0, 1.0 },   // CCC_CONTROLLED : Yellow
                                                 {0.0, 1.0, 0.0, 1.0 } }; // CCCC_BEACON    : Green
    const static unsigned num_category_colors = sizeof(category_color) / 4 / sizeof(GLfloat);

    const static GLfloat disabled_color[] = { 0.2, 0.2, 0.2, 1.0 };
    const static GLfloat new_cat_color[]  = { 1.0, 0.5, 0.0, 1.0 };
    const static GLfloat center_color[]   = { 1.0, 1.0, 1.0, 1.0 };
    const static GLfloat cog_color[]      = { 1.0, 0.0, 0.0, 1.0 };

    ADD_STATIC_CONSOLE_VAR(bool, render_sensors, false);
    
    for (std::list<OdeRigidBody*>::const_iterator it=body_.begin();
         it != body_.end();
         ++it)
    {
        const OdeRigidBody * cur_body = *it;
            
        glPushMatrix();
        
        // First render the center of the rigid body.
        Matrix trans = cur_body->getTransform();
        glMultMatrixf((GLfloat*)&trans._11);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, center_color);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, center_color);
        glutSolidSphere(CENTER_COG_RENDER_RADIUS, 10, 10);

        // Geoms are relative to the object's COG. Render cog.
        Matrix cog_offset(true);
        cog_offset.getTranslation() = cur_body->getCog();
        glMultMatrixf((GLfloat*)&cog_offset._11);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, cog_color);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, cog_color);
        glutSolidSphere(CENTER_COG_RENDER_RADIUS, 10, 10);

        glPopMatrix();


        // Enable wireframe if body is sleeping
        glPushAttrib(GL_POLYGON_BIT | GL_LINE_BIT);
        if (cur_body->isSleeping())
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(2);
        }
        
        for (std::vector<OdeGeom*>::const_iterator it2 = cur_body->getGeoms().begin();
             it2 != cur_body->getGeoms().end();
             ++it2)
        {
            const OdeGeom * cur_geom = *it2;

            if (!render_sensors && cur_geom->isSensor()) continue;

            const GLfloat * color;
            if (!cur_geom->isEnabled())
            {
                color = disabled_color;
            } else
            {
                if (cur_geom->getCategory() < num_category_colors)
                {
                    color = category_color[cur_geom->getCategory()];
                } else
                {
                    color = new_cat_color;
                }
            }
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);


            
            glPushMatrix();
            Matrix offset = cur_geom->getTransform();
            glMultMatrixf((GLfloat*)&offset._11);

            cur_geom->render();

            glPopMatrix();
        }

        glPopAttrib();
    }
#endif
#endif
}



//------------------------------------------------------------------------------
OdeRigidBody * OdeSimulator::instantiate(const OdeRigidBody * blueprint)
{    
    OdeRigidBody * ret = blueprint->instantiate(dBodyCreate(world_id_), this);

    body_.push_back(ret);
    
    return ret;
}

//------------------------------------------------------------------------------
/**
 *  Remove the body from the internal body list.
 */
void OdeSimulator::removeBody(const OdeRigidBody * body)
{  
    for (std::list<OdeRigidBody*>::iterator it = body_.begin();
        it != body_.end();
        ++it)
    {
        if (*it == body)
        {
            body_.erase(it);
            return;
        }
    }

    assert(!"Tried to remove nonexisting body from simulator");
}


//------------------------------------------------------------------------------
/**
 *  A geom might be in both simulator's cur_colliding_geoms_ or
 *  prev_colliding_geoms_ sets. On deletion, this can lead to problems
 *  if the geom stays in one of the sets and a stop event is created
 *  afterwars -> disable geom in both spaces.
 */
void OdeSimulator::disableGeom(OdeGeom * geom)
{
    static_space_->disableGeom(geom);
    actor_space_->disableGeom(geom);
}


//------------------------------------------------------------------------------
void OdeSimulator::addContinuousGeom(OdeContinuousGeom * g)
{
    cont_geom_.push_back(g);
}


//------------------------------------------------------------------------------
void OdeSimulator::removeContinuousGeom(OdeContinuousGeom * g)
{
    for (std::vector<OdeContinuousGeom*>::iterator it = cont_geom_.begin();
        it != cont_geom_.end();
        ++it)
    {
        if (*it == g)
        {
            cont_geom_.erase(it);
            return;
        }
    }
    
    s_log << Log::error
          << "Attempt to remove nonexisting OdeContinuousGeom\n";
}

//------------------------------------------------------------------------------
void OdeSimulator::addContactJoint(const dContact & contact,
                                   const OdeRigidBody * body1,
                                   const OdeRigidBody * body2)
{
    dJointID joint_id = dJointCreateContact(world_id_, contact_group_id_, &contact);

    dJointAttach(joint_id,
                 body1 ? body1->getId() : NULL,
                 body2 ? body2->getId() : NULL);
}


//------------------------------------------------------------------------------
OdeCollisionSpace * OdeSimulator::getStaticSpace()
{
    return static_space_;
}


//------------------------------------------------------------------------------
    OdeCollisionSpace * OdeSimulator::getActorSpace()
{
    return actor_space_;
}


//------------------------------------------------------------------------------
dWorldID OdeSimulator::getWorldId() const
{
    return world_id_;
}

//------------------------------------------------------------------------------
bool OdeSimulator::isEmpty() const
{
    return body_.empty();
}


//------------------------------------------------------------------------------
/**
 *  Write all bodies with their state, as well as their geoms + state
 *  into the logfile.
 */
void OdeSimulator::dumpContents() const
{
    s_log << "\n("
          << instance_count_
          << ") OdeSimulator \""
          << name_
          << "\" : \n";

    for (std::list<OdeRigidBody*>::const_iterator cur_body = body_.begin();
         cur_body != body_.end();
         ++cur_body)
    {
        s_log << **cur_body << "\n";
    }

    
    s_log << "\n";
}    


//------------------------------------------------------------------------------
/**
 *  Applies linear and angular dampening and caps the body velocities
 *  if they are beyond a given threshold.
 */
void OdeSimulator::handleBodyVelocities()
{
    for (std::list<OdeRigidBody*>::iterator it = body_.begin();
         it != body_.end();
         ++it)
    {
        OdeRigidBody * cur_body = *it;

        
        if (cur_body->isStatic()) continue;
        
        Vector v = cur_body->getGlobalLinearVel();        
        Vector w = cur_body->getLocalAngularVel();

        if (abs(v.x_) > MAX_VELOCITY_COMPONENT ||
            abs(v.y_) > MAX_VELOCITY_COMPONENT ||
            abs(v.z_) > MAX_VELOCITY_COMPONENT)
        {
            v /= std::max(std::max(abs(v.x_), abs(v.y_)), abs(v.z_));
            v *= MAX_VELOCITY_COMPONENT;

            cur_body->setGlobalLinearVel(v);

            s_log << Log::debug('p')
                  << "Capping linear velocity for "
                  << cur_body->getName()
                  << " which is at "
                  << cur_body->getTransform().getTranslation()
                  << " and belongs to ";

            RigidBody * body = ((RigidBody*)cur_body->getUserData());
            if (body) s_log << *body << "\n";
            else      s_log << "No Body\n";
        } else
        {
            // Apply velocity dampening
            Vector dampening = -lin_dampening_ * cur_body->getMass()*v;
            if (cur_body->isBelowWater()) dampening *= water_dampening_factor_;
            cur_body->addGlobalForce(dampening);
        }

        if (abs(w.x_) > MAX_ANG_VELOCITY_COMPONENT ||
            abs(w.y_) > MAX_ANG_VELOCITY_COMPONENT ||
            abs(w.z_) > MAX_ANG_VELOCITY_COMPONENT)
        {
            w /= std::max(std::max(abs(w.x_), abs(w.y_)), abs(w.z_));
            w *= MAX_ANG_VELOCITY_COMPONENT;

            cur_body->setLocalAngularVel(w);

            s_log << Log::debug('p')
                  << "Capping angular velocity for "
                  << cur_body->getName()
                  << " which is at "
                  << cur_body->getTransform().getTranslation()
                  << " and belongs to ";

            RigidBody * body = ((RigidBody*)cur_body->getUserData());
            if (body) s_log << *body << "\n";
            else      s_log << "No Body\n";
        } else
        {
            // Apply angular dampening
            w = cur_body->getInertiaTensor().transformVector(w);            
            if (cur_body->isBelowWater()) w *= water_dampening_factor_;
            cur_body->addLocalTorque(-ang_dampening_ * w);
        }
    }
}


//------------------------------------------------------------------------------
void OdeSimulator::enableCategoryCollisions(unsigned cat1, unsigned cat2, bool b)
{
    assert(body_.empty()); // category and collide fields are set at
                           // body creation time, changes after
                           // creation won't be reflected.
    assert(cat1 < 32);
    assert(cat2 < 32);

    if (b)
    {
        category_collide_flag_[cat1] |= 1 << cat2;
        category_collide_flag_[cat2] |= 1 << cat1;
    } else
    {
        category_collide_flag_[cat1] &= ~(1 << cat2);
        category_collide_flag_[cat2] &= ~(1 << cat1);
    }
        
}

//------------------------------------------------------------------------------
/**
 *  Enables / disables collisions of the specified category with all
 *  other categories.
 */
void OdeSimulator::enableCategoryCollisions(unsigned cat, bool b)
{
    assert(body_.empty()); // category and collide fields are set at
                           // body creation time, changes after
                           // creation won't be reflected.
    
    assert(cat < 32);

    for (unsigned i=0; i<32; ++i)
    {
        if (b) category_collide_flag_[i] |=   1<<cat;
        else   category_collide_flag_[i] &= ~(1<<cat);
    }

    category_collide_flag_[cat] = b ? 0xffffff : 0x0;
}

//------------------------------------------------------------------------------
/**
 *  Returns the categories the specified category collides with.
 */
uint32_t OdeSimulator::getCollideFlags(unsigned category) const
{
    assert(category < 32);

    return category_collide_flag_[category];
}


//------------------------------------------------------------------------------
void OdeSimulator::handleContinousGeoms()
{
    for (std::vector<OdeContinuousGeom*>::iterator it = cont_geom_.begin();
         it != cont_geom_.end();
         ++it)
    {
        (*it)->frameMove();
    }
}


} // namespace physics
