#ifndef BBM_OSG_CONVERTER_INCLUDED
#define BBM_OSG_CONVERTER_INCLUDED


#include <stack>
#include <string>

#include <osg/Referenced>
#include <osg/ref_ptr>
#include <osg/NodeVisitor>

#include "BbmImporter.h"


class InstanceProxy;

namespace osg
{
    class Node;
    class Group;
    class MatrixTransform;
    class Geode;
    class Geometry;
}



//------------------------------------------------------------------------------
class GeometryUserData : public osg::Referenced
{
 public:
    GeometryUserData() :
        flags_(0),
        shader_("default") {}
    
    uint16_t flags_; ///< A combination of BBM_MESH_OPTIONs
    std::string shader_;
};

//------------------------------------------------------------------------------
/**
 *  User data for nodes. Stores groups this node must belong to in
 *  order to be visible, as well as currently activated groups.
 */
class NodeUserData : public osg::Referenced
{
 public:
    bool enableGroup(const std::string & group, bool e);
    bool isActive() const;

    bool isGroupMember(const std::string & name) const;

    void addLodsIfNeccessary();

    /// The groups this node belongs to. Conjunction of disjunctions.
    std::vector<std::vector<std::string> > groups_;
    /// The groups currently enabled for this node.
    std::vector<std::string> enabled_groups_;
};


//------------------------------------------------------------------------------
/**
 *  Only created for the top-level MatrixTransform of an imported
 *  object, deleted upon adding it to the scenegraph via addNode. Used
 *  to pass properties to the scenemanager.
 */
class RootNodeUserData : public osg::Referenced
{
 public:
    /// Default values are applied to any node added without root user data!
    RootNodeUserData() :
        flags_(0) {}

    uint16_t flags_; ///< A combination of BBM_NODE_OPTIONs
    std::vector<float> lod_dist_; ///< Determines for each lod level
                                  ///the distance at which it is
                                  ///switched.
};


//------------------------------------------------------------------------------
/**
 *  Installs the shaders based on the node and mesh flags. Currently
 *  could be merged with BbmOsgVisitor...
 */
/*
class CreateShaderVisitor : public osg::NodeVisitor
{
 public:
    CreateShaderVisitor();
    virtual ~CreateShaderVisitor();


    virtual void apply (osg::MatrixTransform & node);
    virtual void apply (osg::Geode & geode);
    
 protected:
    uint16_t root_node_flags_;
};
*/


//------------------------------------------------------------------------------
/**
 *  Converts a BBM into an OSG node tree.
 *
 *  OSG user data pointers at node and geode level are used to store
 *  node properties in the corresponding user data structures (such as
 *  mesh flags, flags pertaining to the whole object (which are stored
 *  in the osg root user data), and group information).
 *
 *  User detail settings are taken into account here, by masking the
 *  flags that actually are written into the osg user data structures.
 */
class BbmOsgVisitor : public bbm::NodeVisitor
{
    friend class StatesetCacheCleaner;
 public:
    BbmOsgVisitor();
    virtual ~BbmOsgVisitor();
    
    virtual void apply(bbm::Node*);
    virtual void apply(bbm::MeshNode*);
    virtual void apply(bbm::GroupNode*);
    virtual void apply(bbm::EffectNode*);

    virtual void pop();

    osg::Node * getOsgRoot() const;


    
 protected:
    
    void addMesh(bbm::Mesh * mesh, osg::Geode * geode);

    void createGroupUserData(const bbm::Node * bbm_node, osg::Node * osg_node);
    
    void createStateSet     (const bbm::Mesh * mesh, osg::Geometry * geom);
    void createGeometry     (const bbm::Mesh * mesh, osg::Geometry * geom);

    void createShaders(osg::Geode * geode);
    
    uint16_t getMaskedMeshFlags(const bbm::Mesh * mesh);


    uint16_t root_node_flags_;
    uint16_t node_flags_mask_;
    uint16_t mesh_flags_mask_;
    
    osg::ref_ptr<osg::MatrixTransform> root_;

    std::stack<osg::Group*> cur_node_;

    std::string bbm_name_; ///< Used as texture directory

    typedef std::map<std::string, osg::StateSet*> StatesetMap;
    static StatesetMap state_set_cache_;
};

#endif
