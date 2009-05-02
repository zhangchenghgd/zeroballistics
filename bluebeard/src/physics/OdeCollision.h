
#ifndef BLUEBEARD_ODE_COLLISION_INCLUDED
#define BLUEBEARD_ODE_COLLISION_INCLUDED

#include <ode/ode.h>

#include <loki/Functor.h>

#include "Vector.h"
#include "Matrix.h"
#include "Plane.h"

namespace physics
{

class OdeRigidBody;
class OdeGeom;
class OdeCollisionSpace;
class OdeSimulator; 


//------------------------------------------------------------------------------
enum COLLISION_TYPE
{
    CT_START,
    CT_IN_PROGRESS,
    CT_STOP,
    CT_SINGLE ///< Generated when colliding a single geom against a space
};


//------------------------------------------------------------------------------    
struct CollisionInfo
{
    CollisionInfo()
        {
            memset(this, 0, sizeof(CollisionInfo));
        }

    OdeGeom * this_geom_;
    OdeGeom * other_geom_;

    Vector pos_;
    Vector n_;
    float penetration_;

    COLLISION_TYPE type_;
};


//------------------------------------------------------------------------------
enum CONTACT_GENERATION
{
    CG_GENERATE_NONE   = 0,
    CG_GENERATE_FIRST  = 1,
    CG_GENERATE_SECOND = 2,
    CG_GENERATE_BOTH   = 3
};


//------------------------------------------------------------------------------
/**
 *  This wrapper allows to still have collision callbacks which only
 *  return true / false.
 */
class ContactGenerationWrapper
{
 public:
    ContactGenerationWrapper(bool b): val_(b ? CG_GENERATE_BOTH : CG_GENERATE_NONE) {}
    ContactGenerationWrapper(CONTACT_GENERATION g): val_(g) {}

    operator CONTACT_GENERATION() const { return val_; }

    CONTACT_GENERATION val_;
};

/// Returns whether contact joints should be created for the
/// collision event.
typedef Loki::Functor<ContactGenerationWrapper, LOKI_TYPELIST_1(const CollisionInfo &)> CollisionCallback;


//------------------------------------------------------------------------------
struct Material
{
    Material() :
        bounciness_(0), mass_(1), friction_(1) {}
    float bounciness_;
    float mass_;
    float friction_;
};


//------------------------------------------------------------------------------
struct TrimeshFace
{
    int32_t v1_;
    int32_t v2_;
    int32_t v3_;
};
std::istream & operator>>(std::istream & in, TrimeshFace & f);


//------------------------------------------------------------------------------
struct Trimesh
{
    Trimesh();
    virtual ~Trimesh();

    void buildTrimesh();
    
    std::vector<Vector>      vertex_data_;
    std::vector<TrimeshFace> index_data_;

    dTriMeshDataID trimesh_id_;    
};




//------------------------------------------------------------------------------
enum GEOM_TYPE
{
    GT_FIRST,
    GT_SPHERE = GT_FIRST,
    GT_CCYLINDER,
    GT_BOX,
    GT_TRIMESH,
    GT_PLANE,
    GT_RAY,
    GT_CONTINUOUS,
    GT_HEIGHTFIELD,
    GT_LAST
};
const unsigned NUM_GEOM_TYPES = GT_LAST - GT_FIRST;


//------------------------------------------------------------------------------
/**
 *  The transform geom's user-data pointer points to objects of this type.
 */
class OdeGeom
{
    friend class OdeModelLoader;
 public:
    OdeGeom();
    virtual ~OdeGeom();

    void setName(const std::string & name);
    const std::string & getName() const;

    void setSensor(bool b);
    bool isSensor() const;

    bool isMassOnly() const;
    
    dGeomID getId() const;
    
    virtual GEOM_TYPE getType() const = 0;

    virtual void render () const = 0;
    
    void enable(bool e);
    bool isEnabled() const;

    bool isStatic() const;

    void setPosition (const Vector & pos);
    void setTransform(const Matrix & mat);
    Matrix getTransform() const;

    void setOffset(const Matrix & o);
    const Matrix & getOffset() const;
    
    void setCategory(unsigned cat, OdeSimulator * sim = NULL);
    unsigned getCategory() const;

    const Material & getMaterial() const;
    
/**
 *  Creates a real ode geom from the information stored in this geom.
 */
    virtual OdeGeom * instantiate() const = 0;

    virtual void setBody(OdeRigidBody * body);
    OdeRigidBody * getBody();

    virtual dMass getMass() const = 0;
    void setMass(float mass);
    
    void * getUserData() const;

    void stopTrackingRigidbody();

    void setSpace(OdeCollisionSpace * new_space);
    OdeCollisionSpace * getSpace();
    const OdeCollisionSpace * getSpace() const;

    void setCollisionCallback(const CollisionCallback & callback);
    const CollisionCallback * getCollisionCallback() const;
    void clearCollisionCallback();
    

 protected:

    OdeGeom(const OdeGeom & other);

    void encapsulateGeom(const Matrix & transform);

    static dGeomID createRayGeom(dSpaceID space_id, float length);
    
    std::string name_;
    
    Material material_;
    bool is_sensor_;
    bool mass_only_;///< only affect mass of rigidbody, is discarded
                    ///afterwards.

    Matrix offset_; ///< The offset of this geom w.r.t the COG of the
                    ///body.

    OdeRigidBody * body_; ///< The body this geom belongs to.

    OdeCollisionSpace * space_; ///< The collision space the geom resides in.

    std::auto_ptr<CollisionCallback> collision_callback_;    
    
    dGeomID id_;  ///< The geom id of the transform geom encapsulating
                  ///the real one. Zero for "blueprint",
                  ///uninstantiated geoms.
};




//------------------------------------------------------------------------------
class OdeSphereGeom : public OdeGeom
{
    friend class OdeModelLoader;
 public:
    OdeSphereGeom(float radius, bool create_ode_geom = true);

    virtual GEOM_TYPE getType() const { return GT_SPHERE; }
    virtual void render() const;
    virtual OdeGeom * instantiate() const;

    virtual dMass getMass() const;
    
    float getRadius() const;
 protected:


    float radius_;
};


//------------------------------------------------------------------------------
class OdeCCylinderGeom : public OdeGeom
{
    friend class OdeModelLoader;
 public:
    OdeCCylinderGeom(float radius, float length, bool create_ode_geom = true);

    virtual GEOM_TYPE getType() const { return GT_CCYLINDER; }
    virtual void render() const;
    virtual OdeGeom * instantiate() const;

    virtual dMass getMass() const;

    float getRadius() const;
    float getLength() const;
    
 protected:

    float radius_;
    float length_;
};


//------------------------------------------------------------------------------
class OdeBoxGeom : public OdeGeom
{
    friend class OdeModelLoader;
 public:

    virtual GEOM_TYPE getType() const { return GT_BOX; }
    virtual void render() const;
    virtual OdeGeom * instantiate() const;

    virtual dMass getMass() const;
    
 protected:
    OdeBoxGeom(float x, float y, float z) :
        x_(x), y_(y), z_(z) {}


    float x_;
    float y_;
    float z_;
};


//------------------------------------------------------------------------------
class OdePlaneGeom : public OdeGeom
{
    friend class OdeModelLoader;
 public:

    virtual GEOM_TYPE getType() const { return GT_PLANE; }
    virtual void render() const;
    virtual OdeGeom * instantiate() const;
    virtual void setBody(OdeRigidBody * body);

    virtual dMass getMass() const;

    void setTransform(const Matrix & transform);
    
 protected:
    OdePlaneGeom(float a, float b, float c, float d) :
        plane_(a,b,c,d) {}

    /// Attention: Ode plane equation goes ax + by +cz = d.
    /// Ours is ax + by + cz + d = 0
    Plane plane_;
};

//------------------------------------------------------------------------------
class OdeTrimeshGeom : public OdeGeom
{
    friend class OdeModelLoader;
 public:

    virtual ~OdeTrimeshGeom();

    virtual GEOM_TYPE getType() const { return GT_TRIMESH; }
    virtual void render() const;
    virtual OdeGeom * instantiate() const;

    virtual void setBody(OdeRigidBody * body);
    virtual dMass getMass() const;
 protected:
    OdeTrimeshGeom(Trimesh * trimesh) :
        trimesh_(trimesh) {}

    Trimesh * trimesh_; ///< Valid only for blueprint trimeshes:
                        ///pointer to the trimesh description
};


//------------------------------------------------------------------------------
class OdeRayGeom : public OdeGeom
{
    friend class OdeModelLoader;
 public:
    OdeRayGeom(float length, bool create_ode_ray = true);

    virtual GEOM_TYPE getType() const { return GT_RAY; }
    virtual void render() const;
    virtual OdeGeom * instantiate() const;

    virtual dMass getMass() const;
    
    float getLength() const;
    void setLength(float l);
    
    void set(const Vector & pos, const Vector & dir);
    void get(Vector & pos, Vector & dir);
    
 protected:    

    float length_;
};

//------------------------------------------------------------------------------
class OdeContinuousGeom : public OdeGeom
{
    friend class OdeModelLoader;
 public:
    virtual ~OdeContinuousGeom();

    virtual GEOM_TYPE getType() const { return GT_CONTINUOUS; }
    virtual void render() const;
    virtual OdeGeom * instantiate() const;

    virtual void setBody(OdeRigidBody * body);

    virtual dMass getMass() const;
    
    void frameMove();
    
 protected:
    OdeContinuousGeom();

    Vector prev_pos_;
};




//------------------------------------------------------------------------------
/**
 *  Currently uses material default values, cannot be instantiated.
 */
class OdeHeightfieldGeom : public OdeGeom
{
 public:
    OdeHeightfieldGeom(unsigned res_x, unsigned res_z,
                       float horz_scale,
                       float min_height, float max_height,
                       const float * data,
                       OdeCollisionSpace * space);
    virtual ~OdeHeightfieldGeom();

    virtual GEOM_TYPE getType() const { return GT_HEIGHTFIELD; }
    virtual void render () const {}    

    virtual OdeGeom * instantiate() const { assert(false); return NULL; }

    virtual dMass getMass() const;
    
 protected:
    dHeightfieldDataID heightfield_data_id_;

    float dim_x_;
    float dim_z_;
};


std::ostream & operator<<(std::ostream & out, const OdeGeom & geom);
std::ostream & operator<<(std::ostream & out, const OdeGeom * geom);



}  // namespace physics

#endif
