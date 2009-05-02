
#include "OdeCollision.h"

#include <limits>


#ifdef ENABLE_DEV_FEATURES
#include <GL/glut.h> // For rendershapes only
#endif

#include <loki/static_check.h>


#include "physics/OdeSimulator.h"

#include "OdeRigidBody.h"
#include "Log.h"
#include "TextValue.h"

#undef min
#undef max

namespace physics
{

    
const std::string HEIGHTFIELD_GEOM_NAME = "heightfield"; 
const float HEIGHTFIELD_THICKNESS = 5.0f;

//------------------------------------------------------------------------------
const char * GEOM_TYPE_NAME[] =
{
    "Sphere",
    "Capped Cylinder",
    "Box",
    "Trimesh",
    "Plane",
    "Ray",
    "Continuous",
    "Heightfield"
};
    

//------------------------------------------------------------------------------
std::istream & operator>>(std::istream & in, TrimeshFace & f)
{
    std::vector<int32_t> values;
    ::operator>>(in,values);
    if (values.size() != 3) throw Exception("Invalid number of elements for trimesh face.");

    f.v1_ = values[0];
    f.v2_ = values[1];
    f.v3_ = values[2];

    return in;    
}


//------------------------------------------------------------------------------
Trimesh::Trimesh() : trimesh_id_(0)
{
}

//------------------------------------------------------------------------------
Trimesh::~Trimesh()
{
    assert(trimesh_id_);
    dGeomTriMeshDataDestroy(trimesh_id_);
}


//------------------------------------------------------------------------------
void Trimesh::buildTrimesh()
{
    trimesh_id_ = dGeomTriMeshDataCreate();
    dGeomTriMeshDataBuildSingle(trimesh_id_,
                                &vertex_data_[0].x_,  sizeof(Vector),       vertex_data_.size(),
                                &index_data_[0].v1_ , index_data_.size()*3, sizeof(TrimeshFace));
}
    

//------------------------------------------------------------------------------    
OdeGeom::OdeGeom() :
    name_("unnamed"),
    is_sensor_(false),
    offset_(true),
    body_(NULL),
    space_(NULL),
    id_(0)
{
    LOKI_STATIC_CHECK(NUM_GEOM_TYPES == sizeof(GEOM_TYPE_NAME)/sizeof(char*),
                      update_geom_names);
}

//------------------------------------------------------------------------------
OdeGeom::~OdeGeom()
{
    // Only do cleanup if this is no blueprint geom.
    if (id_)
    {
        if (space_) setSpace(NULL);
    
        // since cleanup was activated, we don't have to destroy the
        // encapsulated geom.
        dGeomDestroy(id_);
    } else assert(space_ == NULL);
}


//------------------------------------------------------------------------------
void OdeGeom::setName(const std::string & name)
{
    name_ = name;
}
    
//------------------------------------------------------------------------------.
const std::string & OdeGeom::getName() const
{
    return name_;
}


//------------------------------------------------------------------------------
void OdeGeom::setSensor(bool b)
{
    is_sensor_ = b;
}

    
//------------------------------------------------------------------------------
bool OdeGeom::isSensor() const
{
    return is_sensor_;
}


//------------------------------------------------------------------------------
dGeomID OdeGeom::getId() const
{
    return id_;
}

//------------------------------------------------------------------------------
/**
 *  Disables or enables this geom, thus enabling / disabling collision
 *  detection.
 */
void OdeGeom::enable(bool e)
{
    if (e) dGeomEnable (id_);
    else   dGeomDisable(id_);
}

//------------------------------------------------------------------------------
bool OdeGeom::isEnabled() const
{
    return dGeomIsEnabled(id_);
}

//------------------------------------------------------------------------------
/**
 *  
 */
bool OdeGeom::isStatic() const
{
    return !body_ || body_->isStatic();
}

//------------------------------------------------------------------------------
void OdeGeom::setPosition(const Vector & pos)
{
    dGeomSetPosition(id_, pos.x_, pos.y_, pos.z_);
}


//------------------------------------------------------------------------------
void OdeGeom::setTransform(const Matrix & mat)
{
    setPosition(mat.getTranslation());

    dMatrix3 rot;

    rot[0]  = mat._11;
    rot[1]  = mat._12;
    rot[2]  = mat._13;
               
    rot[4]  = mat._21;
    rot[5]  = mat._22;
    rot[6]  = mat._23;
               
    rot[8]  = mat._31;
    rot[9]  = mat._32;
    rot[10] = mat._33;
    
    dGeomSetRotation(id_, rot);
}


//------------------------------------------------------------------------------
/**
 *  Returns the actual position of the geom, taking into account the
 *  rigidbody's transform and the offset transform. Only makes sense
 *  for instantiated geoms.
 */
Matrix OdeGeom::getTransform() const
{
    assert(id_);
    
    Matrix m;
    
    const dReal * rot;
    const dReal * pos;


    if (getType() != GT_PLANE)
    {
        rot = dGeomGetRotation(id_);
        pos = dGeomGetPosition(id_);
    } else
    {
        assert(body_->getId());
        
        rot = dBodyGetRotation(body_->getId());
        pos = dBodyGetPosition(body_->getId());
    }

    
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


    return m*offset_;
}


//------------------------------------------------------------------------------
void OdeGeom::setOffset(const Matrix & o)
{
    offset_ = o;
}
    
    
//------------------------------------------------------------------------------
/**
  *  Returns the geom's offset with respect to the center of gravity.
  */
const Matrix & OdeGeom::getOffset() const
{
    return offset_;
}


//------------------------------------------------------------------------------
/**
 *  Sets the category this geom resides in and the categories this
 *  geom collides with.
 *
 *  \param The new collision category.
 *
 *  \param sim Needed if this geom doesn't belong to a
 *  body. Determines which other categories this geom will collide
 *  with.
 */
void OdeGeom::setCategory(unsigned cat, OdeSimulator * sim)
{
    assert(cat < 32);
    assert(sim || body_);

    if (body_) sim = body_->getSimulator();

    uint32_t cat_flags     = 1 << cat;
    uint32_t collide_flags = sim->getCollideFlags(cat);

    dGeomSetCategoryBits(id_, cat_flags);
    dGeomSetCollideBits (id_, collide_flags);
}

//------------------------------------------------------------------------------
unsigned OdeGeom::getCategory() const
{
    unsigned cat = dGeomGetCategoryBits(id_);
    assert(isPowerOfTwo(cat) || ~cat == 0); // default category is 0xffffffff
    return ld(cat);
}

//------------------------------------------------------------------------------
const Material & OdeGeom::getMaterial() const
{
    return material_;
}


//------------------------------------------------------------------------------
void OdeGeom::setBody(OdeRigidBody * body)
{
    assert(body_ == NULL || body == NULL);

    if (body)
    {
        encapsulateGeom(offset_);
    }
    
    body_ = body;
    dGeomSetBody(id_, body ? body->getId() : 0);

}

//------------------------------------------------------------------------------
OdeRigidBody * OdeGeom::getBody()
{
    return body_;
}

//------------------------------------------------------------------------------
void OdeGeom::setMass(float mass)
{
    material_.mass_ = mass;
}


//------------------------------------------------------------------------------
void * OdeGeom::getUserData() const
{
    return body_ ? body_->getUserData() : NULL;
}


//------------------------------------------------------------------------------
/**
 *  Don't update the geom's position to reflect the RB position.
 */
void OdeGeom::stopTrackingRigidbody()
{
    dGeomSetBody(id_, 0);
}

//------------------------------------------------------------------------------
/**
 *  Removes the geom from its current space and adds it to the
 *  specified one.
 */
void OdeGeom::setSpace(OdeCollisionSpace * new_space)
{
    assert(id_);
    
    if (new_space == space_) return;

    if (space_)
    {
        space_->disableGeom(this);
        dSpaceRemove(space_->getId(), id_);
    }

    if (new_space)
    {
        dSpaceAdd(new_space->getId(), id_);
    }

    space_ = new_space;
}



//------------------------------------------------------------------------------
OdeCollisionSpace * OdeGeom::getSpace()
{
    return space_;
}


//------------------------------------------------------------------------------
const OdeCollisionSpace * OdeGeom::getSpace() const
{
    return space_;
}



//------------------------------------------------------------------------------
void OdeGeom::setCollisionCallback(const CollisionCallback & callback)
{
    collision_callback_.reset(new CollisionCallback(callback));
}

//------------------------------------------------------------------------------
const CollisionCallback * OdeGeom::getCollisionCallback() const
{
    return collision_callback_.get();
}


//------------------------------------------------------------------------------
void OdeGeom::clearCollisionCallback()
{
    collision_callback_.reset(NULL);
}


//------------------------------------------------------------------------------
OdeGeom::OdeGeom(const OdeGeom & other) :
    name_(other.name_),
    material_(other.material_),
    is_sensor_(other.is_sensor_),
    offset_(other.offset_),
    body_(NULL),
    space_(NULL),
    id_(0)
{
    if (other.collision_callback_.get())
    {
        collision_callback_.reset(new CollisionCallback(*other.collision_callback_.get()));
    }
}


//------------------------------------------------------------------------------
/**
 *  Sets this geom's id to a transform geom which encapsulates the
 *  specified geom with the given transform. The user data pointer of
 *  the transform geom is set to this.
 */
void OdeGeom::encapsulateGeom(const Matrix & transform)
{    
    dGeomID encapsulated_id = id_;
    
    id_ = dCreateGeomTransform(0);

    // If we were assigned to a space previously, remove the
    // encapsulated geom from this space and add the new id to it.
    if (space_)
    {
        dSpaceRemove(space_->getId(), encapsulated_id);
        dSpaceAdd   (space_->getId(), id_);
    }
    
    dGeomTransformSetGeom(id_, encapsulated_id);
    dGeomTransformSetInfo(id_, 1);
    dGeomTransformSetCleanup(id_, 1);

    dGeomSetPosition(encapsulated_id,
                     transform.getTranslation().x_,
                     transform.getTranslation().y_,
                     transform.getTranslation().z_);

    // ode has a different major order than we do, so transpose 3x3
    // component
    dMatrix3 rot = { transform._11, transform._12, transform._13, 0.0f,
                     transform._21, transform._22, transform._23, 0.0f,
                     transform._31, transform._32, transform._33, 0.0f };
    dGeomSetRotation(encapsulated_id, rot);
    
    dGeomSetData(id_, this);
}



//------------------------------------------------------------------------------
/**
 *  Creates an ODE ray with the properties we want:
 *    - Determine closest contact points
 *    - No backfacing contacts
 */
dGeomID OdeGeom::createRayGeom(dSpaceID space_id, float length)
{
    dGeomID ret = dCreateRay(space_id, length);
    // We want the closest contact point, and we don't want backfacing
    // contacts.
    dGeomRaySetParams(ret, false, true);
    dGeomRaySetClosestHit(ret, true);

    return ret;
}


//------------------------------------------------------------------------------
OdeSphereGeom::OdeSphereGeom(float radius, bool create_ode_geom) :
    radius_(radius)
{
    if (create_ode_geom)
    {
        id_ = dCreateSphere(space_ ? space_->getId() : 0, radius_);
        dGeomSetData(id_, this);
    }
}


//------------------------------------------------------------------------------
void OdeSphereGeom::render() const
{
#ifndef DEDICATED_SERVER
#ifdef ENABLE_DEV_FEATURES
    glutSolidSphere(radius_, 10, 10);
#endif
#endif
}


//------------------------------------------------------------------------------
OdeGeom * OdeSphereGeom::instantiate() const
{
    OdeSphereGeom * ret = new OdeSphereGeom(*this);

    ret->id_ = dCreateSphere(0, radius_);
    dGeomSetData(ret->id_, ret);

    return ret;
}


//------------------------------------------------------------------------------
dMass OdeSphereGeom::getMass() const
{
    dMass ret;
    dMassSetSphereTotal(&ret, material_.mass_, radius_);
    return ret;
}


//------------------------------------------------------------------------------
float OdeSphereGeom::getRadius() const
{
    return radius_;
}


//------------------------------------------------------------------------------
OdeCCylinderGeom::OdeCCylinderGeom(float radius, float length,
                                   bool create_ode_geom) :
    radius_(radius),
    length_(length)
{
    if (create_ode_geom)
    {
        id_ = dCreateCCylinder(0, radius_, length_);
        dGeomSetData(id_, this);
    }
}



//------------------------------------------------------------------------------
void OdeCCylinderGeom::render() const
{
#ifndef DEDICATED_SERVER
#ifdef ENABLE_DEV_FEATURES    
    Matrix scale(true);
    scale.scale(2*radius_, 2*radius_, length_+2*radius_);
    
    glMultMatrixf(scale);
    glutSolidSphere(0.5f, 10, 10);
#endif
#endif
}


//------------------------------------------------------------------------------
OdeGeom * OdeCCylinderGeom::instantiate() const
{
    OdeCCylinderGeom * ret = new OdeCCylinderGeom(*this);

    ret->id_ = dCreateCCylinder(0, radius_, length_);
    dGeomSetData(ret->id_, ret);
    
    return ret;
}


//------------------------------------------------------------------------------
dMass OdeCCylinderGeom::getMass() const
{
    dMass ret;
    dMassSetCappedCylinderTotal(&ret, material_.mass_, 3, radius_, length_);
    return ret;
}

//------------------------------------------------------------------------------
float OdeCCylinderGeom::getRadius() const
{
    return radius_;
}

//------------------------------------------------------------------------------
float OdeCCylinderGeom::getLength() const
{
    return length_;
}


//------------------------------------------------------------------------------
void OdeBoxGeom::render() const
{
#ifndef DEDICATED_SERVER
#ifdef ENABLE_DEV_FEATURES
    Matrix scale(true);
    scale.scale(x_, y_, z_);
    glMultMatrixf(scale);
    glutSolidCube(1.0);
#endif
#endif
}



//------------------------------------------------------------------------------
OdeGeom * OdeBoxGeom::instantiate() const
{
    OdeBoxGeom * ret = new OdeBoxGeom(*this);

    ret->id_ = dCreateBox(0, x_, y_, z_);
    dGeomSetData(ret->id_, ret);
    
    return ret;
}


//------------------------------------------------------------------------------
dMass OdeBoxGeom::getMass() const
{
    dMass ret;
    dMassSetBoxTotal(&ret, material_.mass_, x_, y_, z_);
    return ret;
}


//------------------------------------------------------------------------------
void OdePlaneGeom::render() const
{
#ifndef DEDICATED_SERVER
#ifdef ENABLE_DEV_FEATURES    
    
    static const float PLANE_SIZE = 20;
    
    Matrix mat1(true), mat2(true);
    mat1.getTranslation() = Vector(0,0,-plane_.d_);

    Vector cross, up(0,1,0);
    vecCross(&cross, &plane_.normal_, &up);
    if (equalsZero(cross.lengthSqr()))
    {
        mat2.loadOrientation(plane_.normal_, Vector(0,0,1));
    } else
    {
        mat2.loadOrientation(plane_.normal_, up);
    }

                
    Matrix mat3 = mat2*mat1;

    glMultMatrixf(mat3);

    glBegin(GL_QUADS);
    glVertex3f( PLANE_SIZE,  PLANE_SIZE, 0);
    glVertex3f( PLANE_SIZE, -PLANE_SIZE, 0);
    glVertex3f(-PLANE_SIZE, -PLANE_SIZE, 0);
    glVertex3f(-PLANE_SIZE,  PLANE_SIZE, 0);
    glEnd();

#endif
#endif
}

//------------------------------------------------------------------------------
OdeGeom * OdePlaneGeom::instantiate() const
{    
    OdePlaneGeom * ret = new OdePlaneGeom(*this);

    ret->id_    = dCreatePlane(0,
                               plane_.normal_.x_,
                               plane_.normal_.y_,
                               plane_.normal_.z_,
                               -plane_.d_);
    dGeomSetData(ret->id_, ret);

    return ret;
}


//------------------------------------------------------------------------------
void OdePlaneGeom::setBody(OdeRigidBody * body)
{    
    if (!body->isStatic())
    {
        throw Exception("Plane geom in non-static body " + body->getName());
    }
    body_  = body;
}


//------------------------------------------------------------------------------
dMass OdePlaneGeom::getMass() const
{
    return dMass();
}


//------------------------------------------------------------------------------
/**
 *  Planes are non-placeable geoms, so we always have to specify
 *  a,b,c,d in global coordinates. This function applies the given
 *  transform to the base a,b,c,d values.
 */
void OdePlaneGeom::setTransform(const Matrix & transform)
{
    Plane plane_trans = plane_;
    plane_trans.transform(transform);
    dGeomPlaneSetParams (id_,
                         plane_trans.normal_.x_,
                         plane_trans.normal_.y_,
                         plane_trans.normal_.z_,
                         -plane_trans.d_);
}


//------------------------------------------------------------------------------
OdeTrimeshGeom::~OdeTrimeshGeom()
{
    // If this is a blueprint geom, delete the corresponding trimesh
    // data as well.
    if (id_ == 0)
    {
        assert(trimesh_);
        delete trimesh_;
    }
}


//------------------------------------------------------------------------------
void OdeTrimeshGeom::render() const
{
#ifndef DEDICATED_SERVER
#ifdef ENABLE_DEV_FEATURES    
    
    glBegin(GL_TRIANGLES);
    for (unsigned i=0; i<trimesh_->index_data_.size(); ++i)
    {
        const TrimeshFace & cur_face = trimesh_->index_data_[i];
        
        Vector vertex[3];

        vertex[0] = Vector(trimesh_->vertex_data_[cur_face.v1_].x_,
                           trimesh_->vertex_data_[cur_face.v1_].y_,
                           trimesh_->vertex_data_[cur_face.v1_].z_);
        vertex[1] = Vector(trimesh_->vertex_data_[cur_face.v2_].x_,
                           trimesh_->vertex_data_[cur_face.v2_].y_,
                           trimesh_->vertex_data_[cur_face.v2_].z_);
        vertex[2] = Vector(trimesh_->vertex_data_[cur_face.v3_].x_,
                           trimesh_->vertex_data_[cur_face.v3_].y_,
                           trimesh_->vertex_data_[cur_face.v3_].z_);

        Vector ab = vertex[1] - vertex[0];
        Vector ac = vertex[2] - vertex[0];
        Vector n;
        vecCross(&n, &ab, &ac);

        n.safeNormalize();

        glNormal3f(n.x_, n.y_, n.z_);
                    
        for (unsigned v=0; v<3; ++v)
        {
            glVertex3f(vertex[v].x_,
                       vertex[v].y_,
                       vertex[v].z_);
        }


    }
    glEnd();
#endif
#endif
}


//------------------------------------------------------------------------------
OdeGeom * OdeTrimeshGeom::instantiate() const
{
    OdeTrimeshGeom * ret = new OdeTrimeshGeom(*this);    

    assert(trimesh_->trimesh_id_);    
        
    ret->id_ = dCreateTriMesh(0, trimesh_->trimesh_id_, NULL, NULL, NULL);
    dGeomSetData(ret->id_, ret);    
    
    return ret;
    
}


//------------------------------------------------------------------------------
void OdeTrimeshGeom::setBody(OdeRigidBody * body)
{
    if (!body->isStatic())
    {
        throw Exception("Trimesh in non-static body " + body->getName());
    }

    OdeGeom::setBody(body);
}


//------------------------------------------------------------------------------
dMass OdeTrimeshGeom::getMass() const
{
    return dMass();
}

//------------------------------------------------------------------------------
/**
 *  \param create_ode_ray Is false for blueprint rays.
 */
OdeRayGeom::OdeRayGeom(float length, bool create_ode_ray) :
    length_(length)
{
    if (create_ode_ray)
    {
        id_ = createRayGeom(0, length);
        dGeomSetData(id_, this);    
    }
}


//------------------------------------------------------------------------------
void OdeRayGeom::render() const
{
#ifdef ENABLE_DEV_FEATURES    
#ifndef DEDICATED_SERVER
    glPushAttrib(GL_LINE_BIT);

    glLineWidth(4);
    
    glBegin(GL_LINES);
    glVertex3f(0,0,0);
    glVertex3f(0,0,length_);
    glEnd();

    glPopAttrib();
#endif
#endif
}

//------------------------------------------------------------------------------
OdeGeom * OdeRayGeom::instantiate() const
{
    OdeRayGeom * ret = new OdeRayGeom(*this);

    ret->id_ = createRayGeom(0, length_);
    dGeomSetData(ret->id_, ret);

    return ret;
}


//------------------------------------------------------------------------------
dMass OdeRayGeom::getMass() const
{
    return dMass();
}

//------------------------------------------------------------------------------
float OdeRayGeom::getLength() const
{
    return length_;
}

//------------------------------------------------------------------------------
void OdeRayGeom::setLength(float l)
{
    length_ = l;
    dGeomRaySetLength(id_, l);
}

//------------------------------------------------------------------------------
void OdeRayGeom::set(const Vector & pos, const Vector & dir)
{
    dGeomRaySet(id_,
                pos.x_, pos.y_, pos.z_,
                dir.x_, dir.y_, dir.z_);
}


//------------------------------------------------------------------------------
void OdeRayGeom::get(Vector & pos, Vector & dir)
{
    dVector3 p, d;
    dGeomRayGet(id_, p, d);

    pos = Vector(p[0], p[1], p[2]);
    dir = Vector(d[0], d[1], d[2]);
}




//------------------------------------------------------------------------------
OdeContinuousGeom::OdeContinuousGeom() :
    prev_pos_(std::numeric_limits<float>::max(),
              std::numeric_limits<float>::max(),
              std::numeric_limits<float>::max())
{
}


//------------------------------------------------------------------------------
OdeContinuousGeom::~OdeContinuousGeom()
{
    if (id_) body_->getSimulator()->removeContinuousGeom(this);
}



//------------------------------------------------------------------------------
void OdeContinuousGeom::render() const
{
}



//------------------------------------------------------------------------------
OdeGeom * OdeContinuousGeom::instantiate() const
{    
    OdeContinuousGeom * ret = new OdeContinuousGeom(*this);

    ret->id_    = createRayGeom(0, 0.0f);

    // Make sure our ray doesn't collide with anything until frameMove has been called.
    ret->enable(false);

    dGeomSetData(ret->id_, ret);    
    return ret;
}


//------------------------------------------------------------------------------
void OdeContinuousGeom::setBody(OdeRigidBody * body)
{
    body_ = body;
    body->getSimulator()->addContinuousGeom(this);
}


//------------------------------------------------------------------------------
dMass OdeContinuousGeom::getMass() const
{
    return dMass();
}


//------------------------------------------------------------------------------
void OdeContinuousGeom::frameMove()
{
    Vector cur_pos = body_->getPosition();

    if (prev_pos_.x_ != std::numeric_limits<float>::max())
    {
        Vector dir = cur_pos - prev_pos_;
        float l = dir.length();

        if (!equalsZero(l))
        {
            dGeomRaySet(id_,
                        prev_pos_.x_, prev_pos_.y_, prev_pos_.z_,
                        dir.x_, dir.y_, dir.z_);
            dGeomRaySetLength(id_, l);
            
            enable(true);
        }
    } // else ray is still initialized to zero length.

    prev_pos_ = cur_pos;
}



//------------------------------------------------------------------------------
OdeHeightfieldGeom::OdeHeightfieldGeom(unsigned res_x, unsigned res_z,
                                       float horz_scale,
                                       float min_height, float max_height,                                       
                                       const float * data,
                                       OdeCollisionSpace * space)
{
    material_.bounciness_ = 0.0f;
    material_.mass_       = 1.0f;
    material_.friction_   = 1.0f;
    
    name_ = HEIGHTFIELD_GEOM_NAME;
    
    dim_x_ = horz_scale*(res_x-1);
    dim_z_ = horz_scale*(res_z-1);
    
    heightfield_data_id_ = dGeomHeightfieldDataCreate();
    dGeomHeightfieldDataBuildSingle(heightfield_data_id_,
                                    data, false,
                                    dim_x_, dim_z_,
                                    res_x, res_z,
                                    1.0f, 0.0f,
                                    HEIGHTFIELD_THICKNESS,
                                    false);
    

    dGeomHeightfieldDataSetBounds(heightfield_data_id_, min_height, max_height);

    id_ = dCreateHeightfield(space->getId(), heightfield_data_id_, true);    
    dGeomSetPosition( id_,
                      dim_x_ * 0.5f,
                      0.0f,
                      dim_z_ * 0.5f );
    dGeomSetData(id_, this);

    space_ = space;
    body_  = NULL;
}    


//------------------------------------------------------------------------------
OdeHeightfieldGeom::~OdeHeightfieldGeom()
{
    dGeomHeightfieldDataDestroy(heightfield_data_id_);
}


//------------------------------------------------------------------------------
dMass OdeHeightfieldGeom::getMass() const
{
    return dMass();
}


const unsigned GEOM_NAME_COL_SIZE = 17;

//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const OdeGeom & geom)
{
    out << "\"" << std::left << std::setw(GEOM_NAME_COL_SIZE) << (geom.getName() + "\"")
        << " : "
        << std::left << std::setw(15) << GEOM_TYPE_NAME[geom.getType()]
        << " in space "
        << (geom.getSpace() ? geom.getSpace()->getName() : std::string("NULL"))
        << ", category "
        << std::setw(2) << geom.getCategory()
        << ", sensor: "
        << geom.isSensor();
    
    return out;
}



} // namespace physics
