
#include "OdeRigidBody.h"

#include "OdeSimulator.h"



namespace physics
{

//------------------------------------------------------------------------------
OdeRigidBody::OdeRigidBody() :
    name_("unnamed"),
    static_(false),
    cog_(Vector(0,0,0)),
    user_data_(NULL),
    id_(0),
    simulator_(NULL),
    mass_initialized_(false)
{
}

//------------------------------------------------------------------------------
OdeRigidBody::~OdeRigidBody()
{
    if (simulator_)
    {
        // instantiated rigidbody
        assert (id_);
        
        simulator_->removeBody(this);
    } else
    {
        // blueprint rigidbody
        assert(!id_);
    }

    for (unsigned g=0; g<geom_.size(); ++g)
    {
        delete geom_[g];
    }

    if (id_) dBodyDestroy(id_);
}


//------------------------------------------------------------------------------
void OdeRigidBody::setName(const std::string & name)
{
    name_ = name;
}


//------------------------------------------------------------------------------
const std::string & OdeRigidBody::getName() const
{
    return name_;
}

//------------------------------------------------------------------------------
dBodyID OdeRigidBody::getId() const
{
    return id_;
}


//------------------------------------------------------------------------------
void OdeRigidBody::setStatic(bool s)
{
    if (s)
    {
        dBodySetLinearVel(id_, 0,0,0);
        dBodySetAngularVel(id_, 0,0,0);

        changeSpace(simulator_->getActorSpace(), simulator_->getStaticSpace());
    } else
    {
        changeSpace(simulator_->getStaticSpace(), simulator_->getActorSpace());
    }

    static_ = s;
    setSleeping(s);
}

//------------------------------------------------------------------------------
void OdeRigidBody::setAutoDisable(bool b)
{
    dBodySetAutoDisableFlag(id_, b);
}


//------------------------------------------------------------------------------
bool OdeRigidBody::isSleeping() const
{
    // static bodies must not be enabled
    assert(!static_ || !dBodyIsEnabled (id_));
    
    return !dBodyIsEnabled (id_); 
}


//------------------------------------------------------------------------------
void OdeRigidBody::setSleeping(bool s)
{
    // Mustn't wake up static bodies
    assert(s || !static_);

    if (s)
    {        
        dBodySetLinearVel(id_, 0,0,0);
        dBodySetAngularVel(id_, 0,0,0);
    }
    
    if (s) dBodyDisable(id_);
    else dBodyEnable(id_);
}


//------------------------------------------------------------------------------
/**
 *  Enables / disables collision for non-sensor geoms.
 */
// void OdeRigidBody::enableCollision(bool b)
// {
//     for (unsigned i=0; i<geom_.size(); ++i)
//     {
//         if (!geom_[i]->isSensor()) geom_[i]->enable(b);
//     }
// }


//------------------------------------------------------------------------------
const Vector & OdeRigidBody::getCog() const
{
    return cog_;
}



//------------------------------------------------------------------------------
Matrix OdeRigidBody::getTransform() const
{
    assert(id_);
    
    Matrix m;
    
    const dReal * rot = dBodyGetRotation(id_);
    const dReal * pos = dBodyGetPosition(id_);

    m._11 = rot[0];
    m._12 = rot[1];
    m._13 = rot[2];

    m._21 = rot[4];
    m._22 = rot[5];
    m._23 = rot[6];

    m._31 = rot[8];
    m._32 = rot[9];
    m._33 = rot[10];

    m._41 = 0.0f;
    m._42 = 0.0f;
    m._43 = 0.0f;
    m._44 = 1.0f;

    m._14 = pos[0];
    m._24 = pos[1];
    m._34 = pos[2];

    m.getTranslation() -= vecToWorld(cog_);
    
    return m;    
}

//------------------------------------------------------------------------------
Vector OdeRigidBody::getPosition() const
{
    const dReal * pos = dBodyGetPosition(id_);

    Vector ret(pos[0], pos[1], pos[2]);

    ret -= vecToWorld(cog_);

    return ret;
}


//------------------------------------------------------------------------------
Vector OdeRigidBody::getGlobalLinearVel() const
{
    const dReal * v = dBodyGetLinearVel(id_);
    return Vector(v[0], v[1], v[2]);
}


//------------------------------------------------------------------------------
Vector OdeRigidBody::getGlobalAngularVel() const
{
    const dReal * w = dBodyGetAngularVel(id_);
    return Vector(w[0], w[1], w[2]);
}


//------------------------------------------------------------------------------
Vector OdeRigidBody::getGlobalLinearVelAtGlobalPos(const Vector & pos) const
{
    dVector3 res;
    dBodyGetPointVel(id_,
                     pos.x_, pos.y_, pos.z_,
                     &res[0]);
    
    return Vector(res[0], res[1], res[2]);
}


//------------------------------------------------------------------------------
Vector OdeRigidBody::getLocalLinearVel() const
{
    return vecFromWorld(getGlobalLinearVel());
}


//------------------------------------------------------------------------------
Vector OdeRigidBody::getLocalAngularVel() const
{
    return vecFromWorld(getGlobalAngularVel());
}




//------------------------------------------------------------------------------
void OdeRigidBody::setTransform(const Matrix & t)
{
    dMatrix3 m;

    m[0]  = t._11;
    m[1]  = t._12;
    m[2]  = t._13;

    m[4]  = t._21;
    m[5]  = t._22;
    m[6]  = t._23;

    m[8]  = t._31;
    m[9]  = t._32;
    m[10] = t._33;

    dBodySetRotation(id_, m);
    
    Vector corr_pos = vecToWorld( cog_);
    corr_pos += t.getTranslation();
    
    dBodySetPosition(id_, corr_pos.x_, corr_pos.y_, corr_pos.z_);


    // Manually apply transform to any plane geom.
    for (unsigned g=0; g<geom_.size(); ++g)
    {
        if (geom_[g]->getType() == GT_PLANE)
        {
            ((OdePlaneGeom*)geom_[g])->setTransform(t);
        }
    }
}

//------------------------------------------------------------------------------
void OdeRigidBody::setPosition(const Vector & pos)
{
    Vector corr_pos = vecToWorld(cog_);
    corr_pos += pos;
    
    dBodySetPosition(id_, corr_pos.x_, corr_pos.y_, corr_pos.z_);
}


//------------------------------------------------------------------------------
void OdeRigidBody::setGlobalLinearVel(const Vector & v)
{
    assert(!static_);
    
    dBodySetLinearVel(id_, v.x_, v.y_, v.z_);
}

//------------------------------------------------------------------------------
void OdeRigidBody::setGlobalAngularVel(const Vector & w)
{
    assert(!static_);

    dBodySetAngularVel(id_, w.x_, w.y_, w.z_);
}


//------------------------------------------------------------------------------
void OdeRigidBody::setLocalLinearVel(const Vector & v)
{
    assert(!static_);

    setGlobalAngularVel(vecToWorld(v));
}

//------------------------------------------------------------------------------
void OdeRigidBody::setLocalAngularVel(const Vector & w)
{
    assert(!static_);
    
    setGlobalAngularVel(vecToWorld(w));
}


//------------------------------------------------------------------------------
void OdeRigidBody::addGlobalForce (const Vector & force)
{
    assert(!static_);
    
    dBodyAddForce(id_, force.x_, force.y_, force.z_);    
}

//------------------------------------------------------------------------------
void OdeRigidBody::addGlobalForceAtGlobalPos(const Vector & force,
                                             const Vector & pos)
{
    assert(!static_);

    dBodyAddForceAtPos(id_,
                       force.x_, force.y_, force.z_,
                       pos.x_,   pos.y_,   pos.z_);
}


//------------------------------------------------------------------------------
void OdeRigidBody::addGlobalTorque(const Vector & torque)
{
    assert(!static_);
    
    dBodyAddTorque(id_, torque.x_, torque.y_, torque.z_);    
}


//------------------------------------------------------------------------------
void OdeRigidBody::addLocalForce (const Vector & force)
{
    assert(!static_);
    
    dBodyAddRelForce(id_, force.x_, force.y_, force.z_);    
}


//------------------------------------------------------------------------------
void OdeRigidBody::addLocalTorque(const Vector & torque)
{
    assert(!static_);
    
    dBodyAddRelTorque(id_, torque.x_, torque.y_, torque.z_);    
}

//------------------------------------------------------------------------------
void OdeRigidBody::addLocalForceAtLocalPos(const Vector & force, const Vector & pos)
{
    assert(!static_);
    
    dBodyAddRelForceAtRelPos(id_,
                             force.x_, force.y_, force.z_,
                             pos.x_ - cog_.x_,   pos.y_ - cog_.y_,   pos.z_ - cog_.z_);
}


//------------------------------------------------------------------------------
/**
 *  Sets the collision category for all child geoms which are not sensors.
 */
void OdeRigidBody::setCollisionCategory(unsigned cat)
{
    for (unsigned g=0; g<geom_.size(); ++g)
    {
        if (!geom_[g]->isSensor()) geom_[g]->setCategory(cat);
    }
}

    
//------------------------------------------------------------------------------
/**
 *  Sets whether this body is influenced by gravity.
 */
void OdeRigidBody::enableGravity(bool g)
{
    dBodySetGravityMode(id_, g);
}

//------------------------------------------------------------------------------
/**
 *  Adjust the mass of the body to be m, leaving inertial properties
 *  intact.
 */
void OdeRigidBody::setTotalMass(float m)
{
    assert(m>0);
    
    dMass mass;
    dBodyGetMass(id_, &mass);
    dMassAdjust(&mass, m);
    dBodySetMass(id_, &mass);
}


//------------------------------------------------------------------------------
float OdeRigidBody::getMass() const
{
    dMass mass;
    dBodyGetMass( id_, &mass );
    return mass.mass;   
}


//------------------------------------------------------------------------------
Matrix OdeRigidBody::getInertiaTensor() const
{
    Matrix ret(true);

    dMass mass;
    dBodyGetMass( id_, &mass );


    ret._11 = mass.I[0];
    ret._12 = mass.I[1];
    ret._13 = mass.I[2];

    ret._21 = mass.I[4];
    ret._22 = mass.I[5];
    ret._23 = mass.I[6];

    ret._31 = mass.I[8];
    ret._32 = mass.I[9];
    ret._33 = mass.I[10];
    
    return ret;
}


//------------------------------------------------------------------------------
Vector OdeRigidBody::vecToWorld  (const Vector & v) const
{
    Vector ret;

    dVector3 res;
    dBodyVectorToWorld(id_, v.x_, v.y_, v.z_,
                       res);

    ret.x_ = res[0];
    ret.y_ = res[1];
    ret.z_ = res[2];
    
    return ret;    
}


//------------------------------------------------------------------------------
Vector OdeRigidBody::vecFromWorld(const Vector & v) const
{
    Vector ret;

    dVector3 res;
    dBodyVectorFromWorld(id_, v.x_, v.y_, v.z_,
                         res);

    ret.x_ = res[0];
    ret.y_ = res[1];
    ret.z_ = res[2];
    
    return ret;    
}


//------------------------------------------------------------------------------
OdeSimulator * OdeRigidBody::getSimulator()
{
    return simulator_;
}


//------------------------------------------------------------------------------
/**
 *  Creates a copy of this body and its geoms in the given space &
 *  simulator.
 */
OdeRigidBody * OdeRigidBody::instantiate(dBodyID id, OdeSimulator * simulator) const
{
    OdeRigidBody * ret = new OdeRigidBody();

    ret->name_      = name_;
    ret->static_    = static_;
    ret->cog_       = cog_;
    ret->id_        = id;
    ret->simulator_ = simulator;

    for (unsigned g=0; g<geom_.size(); ++g)
    {
        OdeGeom * new_geom = geom_[g]->instantiate();

        // This needs to happen first, as it sets up the encapsulation
        // for the relative transform.
        ret->addGeom(new_geom);
        
        if (!new_geom->isSensor())
        {
            new_geom->setSpace(static_ ? simulator->getStaticSpace() : simulator->getActorSpace());
        }

    }
    
    // Put static rigidbodies to sleep. It would also be possible to
    // not create a rigidbody in this case at all, but at the
    // additional cost of having to set all child geom positions
    // manually.
    if (static_)
    {
        dBodyDisable(ret->id_);
    }
    
    return ret;
}


//------------------------------------------------------------------------------
void OdeRigidBody::addGeom(OdeGeom * geom)
{
    geom->setBody(this);
    geom_.push_back(geom);

    // Add geom's mass if not sensor
    if (!geom->isSensor() && !equalsZero(geom->getMaterial().mass_))
    {
        dMass mass = geom->getMass();

        if (!equalsZero(mass.mass))
        {
            const Matrix & offset = geom->getOffset();
    
            // First apply the offset to the new mass
            dMatrix3 rot = {offset._11, offset._12, offset._13, 0,
                            offset._21, offset._22, offset._23, 0,
                            offset._31, offset._32, offset._33, 0 };
        
            dMassRotate(&mass, rot);
    
            if (mass_initialized_)
            {
                dMass total_mass;
                dBodyGetMass(id_, &total_mass);
                dMassAdd(&total_mass, &mass);
                dBodySetMass(id_, &total_mass);
            } else
            {
                dBodySetMass(id_, &mass);
                mass_initialized_ = true;
            }
        }
    }
}

//------------------------------------------------------------------------------
/**
 *  Returns the first encountered geom with the specified name or
 *  NULL.
 */
OdeGeom * OdeRigidBody::getGeom(const std::string & name)
{
    unsigned ind = getGeomIndex(name);

    return ind==(unsigned)-1 ? NULL : geom_[ind];
}


//------------------------------------------------------------------------------
unsigned OdeRigidBody::getGeomIndex(const std::string & name) const
{
    for (unsigned i=0; i<geom_.size(); ++i)
    {
        if (geom_[i]->getName() == name) return i;
    }
    return (unsigned)-1;
}


//------------------------------------------------------------------------------
const std::vector<OdeGeom*> & OdeRigidBody::getGeoms() const
{
    return geom_;
}

//------------------------------------------------------------------------------
std::vector<OdeGeom*> & OdeRigidBody::getGeoms()
{
    return geom_;
}

//------------------------------------------------------------------------------
void OdeRigidBody::deleteGeom(unsigned index)
{
    assert(index < geom_.size());
    
    delete geom_[index];
    
    std::vector<OdeGeom*>::iterator it = geom_.begin() + index;
    geom_.erase(it); 
}


//------------------------------------------------------------------------------
/**
 *  Completely removes any association betwenn this body and the geom
 *  with the specified index. The geom won't be deleted upon body
 *  destruction anymore, so it must be cleaned up by the caller.
 *
 *  \return A pointer to the detached geom.
 */
OdeGeom * OdeRigidBody::detachGeom(unsigned index)
{    
    if (index >= geom_.size()) return NULL;

    OdeGeom * ret = geom_[index];

    std::vector<OdeGeom*>::iterator it = geom_.begin() + index;
    geom_.erase(it);
    
    ret->setBody(NULL);
    return ret;
}



//------------------------------------------------------------------------------
void * OdeRigidBody::getUserData()
{
    return user_data_;
}

//------------------------------------------------------------------------------
void OdeRigidBody::setUserData(void * data)
{
    user_data_ = data;
}

//------------------------------------------------------------------------------
/**
 *  RigidBodies with a collision callback also need a user data
 *  pointer.
 *
 *  Sets the collision callback for the rigidbody's geoms.
 *
 *  \param callback The function to call on collision.
 *
 *  \param name The name of the geoms for which to set the callback or
 *  "" if all geom's callbacks are to be set.
 *
 *  \return Whether any callback was set.
 */
bool OdeRigidBody::setCollisionCallback(const CollisionCallback & callback,
                                        const std::string & geom_name)
{
    assert(user_data_);

    bool ret = false;

    for (std::vector<OdeGeom*>::iterator it = geom_.begin();
        it != geom_.end();
        ++it)
    {
        if (geom_name == "" || geom_name == (*it)->getName())
        {
            (*it)->setCollisionCallback(callback);
            ret = true;
        }
    }

    return ret;
}


//------------------------------------------------------------------------------
void OdeRigidBody::clearCollisionCallback(const std::string & geom_name)
{
    assert(user_data_);

    for (std::vector<OdeGeom*>::iterator it = geom_.begin();
        it != geom_.end();
        ++it)
    {
        if (geom_name == "" || geom_name == (*it)->getName())
        {
            (*it)->clearCollisionCallback();
        }
    }
}


//------------------------------------------------------------------------------
/**
 *  Moves all geoms in old_space to new_space. Sensors are ignored.
 */
void OdeRigidBody::changeSpace(OdeCollisionSpace * old_space,
                               OdeCollisionSpace * new_space)
{
    for (std::vector<OdeGeom*>::iterator it = geom_.begin();
         it != geom_.end();
         ++it)
    {
        if ((*it)->getSpace() == old_space && !(*it)->isSensor())
        {
            (*it)->setSpace(new_space);
        } 
    }
}
    

const unsigned BODY_NAME_COL_SIZE = 20;

//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const OdeRigidBody & body)
{
    out << "Body \"" << std::left << std::setw(BODY_NAME_COL_SIZE) << (body.getName() + "\"")
        << " : "
        << "sleeping: "
        << body.isSleeping()
        << ", static: "
        << body.isStatic();


    const std::vector<OdeGeom*> & geom = body.getGeoms();

    if (!geom.empty())
    {
        for (unsigned g=0; g<geom.size(); ++g)
        {
            out << "\n\t" << *geom[g];
        }
    }
    
    return out;
}


} // namespace physics
