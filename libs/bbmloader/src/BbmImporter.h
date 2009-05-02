
#ifdef _MSVC
#pragma warning (disable : 4786) //"Bezeichner wurde auf '255' Zeichen in den Debug-Informationen verkürzt"
#endif

#ifndef STUNTS_BBMIMPORTER_INCLUDED
#define STUNTS_BBMIMPORTER_INCLUDED

#include <fstream>
#include <vector>

#include "Singleton.h"


#include "Datatypes.h"
#include "Utils.h"
#include "Matrix.h"

namespace bbm
{


class Mesh;
class NodeVisitor;

//------------------------------------------------------------------------------
enum BBM_CREATOR
{
    BC_BLENDER   = 1,
    BC_NATIVE    = 2,
    BC_UNDEFINED = 3
};

 
//------------------------------------------------------------------------------
enum BBM_MESH_OPTION
{
    BMO_LIGHTING           = 1,
    BMO_CULL_FACES         = 2,
    BMO_ALPHA_TEST         = 4,
    BMO_ALPHA_BLEND        = 8,
    BMO_PER_PIXEL_LIGHTING = 16,

    BMO_LIGHT_MAP          = 32,
    BMO_BUMP_MAP           = 64,
    BMO_EMISSIVE_MAP       = 128
};


//------------------------------------------------------------------------------
enum BBM_NODE_OPTION
{
    BNO_SHADOW_BLOCKER  = 1,
    BNO_SHADOW_RECEIVER = 2,
    BNO_INSTANCED       = 4
};
 
 
//------------------------------------------------------------------------------ 
class Node
{    
 public:
    Node();
    virtual ~Node();

    virtual void load(serializer::Serializer & s,
                      const Matrix * inverse_parent_transform);
    virtual void save(serializer::Serializer & s,
                      const Matrix parent_transform);

    void accept(NodeVisitor & visitor);

    const std::string & getName() const;
    const Matrix & getTransform() const;
    const Matrix & getTotalTransform() const;

    const std::vector<std::vector<std::string> > & getGroups() const;
    
    unsigned getNumChildren() const;

    const std::string & getLodClass() const;
    uint16_t getFlags() const;
    
    static Node * loadFromFile(const std::string & filename);
    void saveToFile(const std::string & full_name);

    BBM_CREATOR getCreator() const;
    
 protected:

    void loadChildren(serializer::Serializer & s,
                      const Matrix * inverse_parent_transform);
    void saveChildren(serializer::Serializer & s);
    
    void loadTransform(serializer::Serializer & s,
                       Matrix & scale,
                       const Matrix * inverse_parent_transform);
    void loadGroups(serializer::Serializer & s);
    void saveGroups(serializer::Serializer & s);
    
    virtual void doAccept(NodeVisitor & visitor);
    
    std::string name_;

    Matrix transform_;       ///< \see Node::loadTransform
    Matrix total_transform_; ///< \see Node::loadTransform    

    /// Used to selectively enable or disable this node, e.g. lod
    /// groups, destroyed/intact, upgrades...
    /// Conjunction of disjunctions.
    std::vector<std::vector<std::string> > groups_;
    
    std::vector<Node* > child_;

    std::string lod_class_;
    uint16_t flags_; ///< Combination of BBM_NODE_OPTION values.

    BBM_CREATOR creator_;

    enum NODE_TYPE
        {
            NT_MESH,
            NT_GROUP,
            NT_EFFECT
        };

};


//------------------------------------------------------------------------------
class MeshNode : public Node
{
 public:
    MeshNode();
    virtual ~MeshNode();

    virtual void load(serializer::Serializer & s,
                      const Matrix * inverse_parent_transform);
    virtual void save(serializer::Serializer & s,
                      const Matrix parent_transform);

    
    const std::vector<Mesh*> & getMeshes() const;
    
 protected:

    virtual void doAccept(NodeVisitor & visitor);    
    
    std::vector<Mesh * > mesh_;
};


//------------------------------------------------------------------------------
class GroupNode : public Node
{
 public:
    GroupNode();
    virtual ~GroupNode();

    virtual void load(serializer::Serializer & s,
                      const Matrix * inverse_parent_transform);
    virtual void save(serializer::Serializer & s,
                      const Matrix parent_transform);

 protected:

    virtual void doAccept(NodeVisitor & visitor);    
};



//------------------------------------------------------------------------------
class EffectNode : public Node
{
 public:
    EffectNode();
    virtual ~EffectNode();

    virtual void load(serializer::Serializer & s,
                      const Matrix * inverse_parent_transform);
    virtual void save(serializer::Serializer & s,
                      const Matrix parent_transform);

 protected:
    virtual void doAccept(NodeVisitor & visitor);    
};



//------------------------------------------------------------------------------
class NodeVisitor
{
 public:
    NodeVisitor();
    virtual ~NodeVisitor();

    virtual void apply(Node*)       = 0;
    virtual void apply(MeshNode*)   = 0;
    virtual void apply(GroupNode*)  = 0;
    virtual void apply(EffectNode*) = 0;

    virtual void pop()              = 0;
    
 protected:
};


//------------------------------------------------------------------------------
/**
 *  A mesh residing in system memory. If a feature (tex coord,
 *  color...) is not present, its array has length zero.
 *
 *  Every different material and/or texture in blender gets assigned
 *  its own mesh, thus there can be more than one mesh in a bbm node.
 */
class Mesh
{
 public:    
    Mesh(serializer::Serializer & s, const Matrix & scale);
    virtual ~Mesh();

    void save(serializer::Serializer & s);
    
    void transform(const Matrix & mat);
    
    const Material & getMaterial() const;
    const std::string & getShader() const;
    
    const std::string & getTextureName() const;
    const std::string & getLmName() const;
    const std::string & getEmName() const;
    

    const std::vector<std::vector<TexCoord> > & getTexData() const;
    const std::vector<Color>    & getColorData() const;
    const std::vector<Vector>   & getNormalData() const;
    const std::vector<Vector>   & getTangentData() const;
    const std::vector<Vector>   & getBiTangentData() const;
    const std::vector<Vector>   & getVertexData() const;
    const std::vector<uint16_t> & getIndexData() const;
    
    std::vector<std::vector<TexCoord> > & getTexData();
    std::vector<Color>    & getColorData();
    std::vector<Vector>   & getNormalData();
    std::vector<Vector>   & getTangentData();
    std::vector<Vector>   & getBiTangentData();
    std::vector<Vector>   & getVertexData();
    std::vector<uint16_t> & getIndexData();

    uint16_t getFlags() const;
    float getParallaxStrength() const;
    float getNormalStrength() const;

    void generateTangentSpaceInfo();

    std::string getUniqueMaterialIdentifier(uint16_t flag_mask,
                                            uint16_t node_flags) const;
    
 protected:


    Material material_;       ///< The material of the mesh.

    std::string shader_;
    
    std::string texture_name_; ///< This mesh's texture name.
    std::string lm_name_; ///< Name of lightmap texture.
    std::string em_name_; ///< Name of emissive texture.
    
    std::vector<std::vector<TexCoord> > tex_data_; /// First array are different texcoord sets.
    std::vector<Vector>                 normal_data_;
    std::vector<Vector>                 tangent_data_;
    std::vector<Vector>                 bi_tangent_data_;
    std::vector<Vector>                 vertex_data_;
    std::vector<uint16_t>               index_data_;

    uint16_t flags_; ///< Combination of BBM_MESH_OPTION values.

    float parallax_strength_;
    float normal_strength_;
};

} // namespace bbm


 
#endif // #ifndef STUNTS_BBMIMPORTER_INCLUDED
