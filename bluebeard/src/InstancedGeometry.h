

#ifndef BLUEBEARD_INSTANCED_GEOMETRY_INCLUDED
#define BLUEBEARD_INSTANCED_GEOMETRY_INCLUDED


#include <stack>
#include <vector>
#include <map>

#include <osg/Geometry>

#include "OsgNodeWrapper.h"

#include "Matrix.h"
#include "RegisteredFpGroup.h"

class Observable;
class RigidBody;
class InstancedPrimitive;
class InstancedGeometryDescription;



//------------------------------------------------------------------------------
/**
 *  The actual per--instance data. Gets written into a large uniform
 *  array to be read in the shader.  Currently is of size 2*vec3.
 */
struct InstanceData
{
    InstanceData() :
        pos_(0,0,0), alpha_(0.0f), scale_(1.0f), diffuse_(1) {}
    
    Vector pos_;
    float alpha_; ///< Rotation around y-axis
    float scale_;
    float diffuse_;
};

const unsigned NUM_VEC3S_PER_INSTANCE = sizeof(InstanceData) / sizeof(osg::Vec3);


//------------------------------------------------------------------------------
class InstanceProxy : public OsgNodeWrapper
{
 public:
    InstanceProxy(InstancedGeometryDescription * description);

    virtual ~InstanceProxy();

    virtual void setLodLevel(unsigned lvl);
    virtual unsigned getLodLevel() const;
    
    virtual const std::vector<float> & getLodDists() const;
    virtual void  setLodDists(const std::vector<float> & dist);
    
    virtual void setTransform(const Matrix & m);
    virtual void addToScene();
    virtual void removeFromScene();

    virtual Vector getPosition() const;
    virtual void setPosition(const Vector & v);

    virtual float getRadius() const;

    virtual osg::MatrixTransform * getOsgNode();

    void setDiffuse(float d);

    InstancedGeometryDescription * getDescription() const;
    
 protected:

    InstancedGeometryDescription * desc_;
    unsigned cur_lod_lvl_;
};



typedef std::vector<InstanceData> InstanceDataContainer;
typedef std::map<const InstanceProxy*, unsigned> PositionMap;


//------------------------------------------------------------------------------
/**
 *  Keeps all information neccessary to draw instances of one object
 *  type (instance data for all proxies, array of InstancedPrimitives
 *  corresponding to materials).
 *
 *  The important thing is that all instance data is stored in one
 *  huge array so it can be directly passed to glUnifom for
 *  performance.
 */
class InstancingInfo
{
 public:

    void         addInstance   (const InstanceProxy * proxy, const InstanceData & instance_data);
    InstanceData removeInstance(const InstanceProxy * proxy);
    bool         hasInstance   (const InstanceProxy * proxy) const;

    InstanceData & getInstanceData(const InstanceProxy * proxy);
    
    std::vector<osg::ref_ptr<InstancedPrimitive> > primitive_; ///< Different groups for different materials.

    /// Stores the proxy for each instance in instance_data_. Required
    /// to efficiently update data structures on proxy removal.
    std::vector<const InstanceProxy*> proxy_;
    
    InstanceDataContainer instance_data_;
    PositionMap position_map_; ///< Stores the index into instance_data_ which belongs to the instance proxy.
};


//------------------------------------------------------------------------------
/**
 *  Geometry that gets instanced. One instanced object can consist of
 *  multiple InstancedPrimitives if it is composed of multiple
 *  materials.
 */
class InstancedPrimitive : public osg::Geometry
{
 public:
    InstancedPrimitive(const osg::Geometry & geometry, const InstancingInfo & lod_info);
    virtual ~InstancedPrimitive();
    
    virtual Object* cloneType() const               { assert(false); return NULL; }
    virtual Object* clone(const osg::CopyOp&) const { assert(false); return NULL; }
    virtual const char* className() const           { return "InstancedPrimitive"; }
    
    virtual osg::BoundingBox computeBound() const;
    virtual void drawImplementation(osg::RenderInfo& renderInfo) const;

    virtual bool supports (const osg::PrimitiveFunctor &) const;
    virtual void accept (osg::PrimitiveFunctor &) const;
    
 protected:

    void replicateGeometry(const osg::Geometry & geometry);

    template <typename T>
    T * replicateArray(const osg::Array * array, unsigned num_replicas);
    
    float instance_radius_;

    const InstancingInfo & info_;
    
//------------------------------------------------------------------------------
/**
 *  Necessary to skip remaining geometry if number of instances is not
 *  multiple of BATCH_SIZE.
 */
    class DrawElementsUIntCapped : public osg::DrawElementsUInt
    {
    public:
        DrawElementsUIntCapped(GLenum mode, unsigned no);

        /// never gets drawn directly by OSG
        virtual void draw( osg::State &, bool) const {assert (false); }
        
        void bindBuffer(osg::State& state);
        void drawCapped(osg::State& state, unsigned num_indices);
    };



    mutable GLint instance_data_uniform_location_;

    mutable unsigned instance_batch_size_;
    
    osg::ref_ptr<DrawElementsUIntCapped> draw_elements_;

    static osg::ref_ptr<osg::Drawable::Extensions> drawable_extensions_;
    static osg::ref_ptr<osg::GL2Extensions>             gl2_extensions_;
};





//------------------------------------------------------------------------------
/**
 *  Contains all InstancedPrimitives needed for one model (all
 *  materials, all lod models)
 */
class InstancedGeometryDescription
{
 public:
    InstancedGeometryDescription(const std::string & name);
    virtual ~InstancedGeometryDescription();

    void addGeometry(const osg::Geometry * geometry, unsigned lod);
    float getRadius() const;

    const std::vector<float> & getLodDists() const;
    void  setLodDists(const std::vector<float> & dist);

    void dirtyBound(unsigned lod);
    
    void         addInstance   (const InstanceProxy * proxy, const InstanceData & instance_data);
    InstanceData removeInstance(const InstanceProxy * proxy);
    bool         hasInstance   (const InstanceProxy * proxy) const;

    InstanceData & getInstanceData(const InstanceProxy * proxy);
    
    osg::Geode * getGeode();
    
 protected:
    std::string name_;


    std::vector<InstancingInfo> lod_info_;

    /// Because instance proxies don't store their instancing data
    /// themselves, put them in here while they are unneeded.
    InstancingInfo              unused_info_; 
    
    osg::ref_ptr<osg::Geode> geode_; ///< All InstancedPrimitives are below this geode.
    
    float radius_;

    std::vector<float> lod_dist_;
};


//------------------------------------------------------------------------------
class InstanceManager
{
 public:    
    InstancedGeometryDescription * getOrCreateInstanceDescription(const std::string & name,
                                                                  const osg::MatrixTransform * node);

    void reset();
    
 protected:

    void addToDescription(InstancedGeometryDescription * desc,
                          const osg::Geode * geode,
                          unsigned lod_lvl) const;
    
    std::map<std::string, InstancedGeometryDescription*> desc_;
};



//------------------------------------------------------------------------------
template <typename T>
T * InstancedPrimitive::replicateArray(const osg::Array * array, unsigned num_replicas)
{
    const T * src_array = dynamic_cast<const T*>(array);
    assert(src_array);
            
    T & new_array = *(new T(num_replicas * src_array->size()));

    for (unsigned b=0; b<num_replicas; ++b)
    {
        memcpy(&new_array[b*src_array->size()],
               &(*src_array)[0],
               src_array->size()*sizeof(new_array[0]));
    }
            
    return &new_array;
}


#endif
