
#include "BbmImporter.h"

#include "Log.h"
#include "Serializer.h"
#include "ParameterManager.h"
#include "NvMeshMender.h"



namespace bbm
{

const uint32_t MAGIC_HEADER = 0xabcafe07;


    

//------------------------------------------------------------------------------    
Node::Node() :
    flags_(0),
    creator_(BC_UNDEFINED)
{
}


//------------------------------------------------------------------------------
Node::~Node()
{
    for (unsigned i=0; i<child_.size(); ++i)
    {
        delete child_[i];
    }
}



//------------------------------------------------------------------------------
void Node::load(serializer::Serializer & s,
                const Matrix * inverse_parent_transform)
{
    uint32_t header;
    s.get(header);
    uint32_t creator;
    s.get(creator); creator_ = (BBM_CREATOR)creator;
    
    if (header != MAGIC_HEADER)
    {
        Exception e;
        e << "Bbm magic header doesn't match. Got "
          << (void*)header
          << ", expected "
          << (void*)MAGIC_HEADER
          << "\n";
        throw e;
    }
    s.get(flags_);
    s.get(lod_class_);

#ifdef ENABLE_DEV_FEATURES
    bool b = false;
    try
    {
        b = s_params.get<bool>("dev.disable_instancing");
    } catch (ParamNotFoundException & e){}
    if (b) flags_ &= ~BNO_INSTANCED;
#endif
    
    loadChildren(s, inverse_parent_transform);
}


//------------------------------------------------------------------------------
void Node::save(serializer::Serializer & s,
                const Matrix parent_transform)
{
    s.put(MAGIC_HEADER);
    s.put((uint32_t)BC_NATIVE);
    s.put(flags_);
    s.put(lod_class_);

    saveChildren(s);
}


//------------------------------------------------------------------------------
void Node::accept(NodeVisitor & visitor)
{
    doAccept(visitor);

    for (unsigned c=0; c<child_.size(); ++c)
    {
        child_[c]->accept(visitor);
    }

    visitor.pop();
}


//------------------------------------------------------------------------------
const std::string & Node::getName() const
{
    return name_;
}

//------------------------------------------------------------------------------
const Matrix & Node::getTransform() const
{
    return transform_;
}


//------------------------------------------------------------------------------
const Matrix & Node::getTotalTransform() const
{
    return total_transform_;
}


//------------------------------------------------------------------------------
const std::vector<std::vector<std::string> > & Node::getGroups() const
{
    return groups_;
}


//------------------------------------------------------------------------------
unsigned Node::getNumChildren() const
{
    return child_.size();
}

//------------------------------------------------------------------------------
const std::string & Node::getLodClass() const
{
    return lod_class_;
}

//------------------------------------------------------------------------------
uint16_t Node::getFlags() const
{
    return flags_;
}





//------------------------------------------------------------------------------
Node * Node::loadFromFile(const std::string & filename)
{
    try
    {
        serializer::Serializer s(filename, serializer::SOM_READ);

        
        std::auto_ptr<Node> ret(new Node());

        // somewhat hacky: filename is the relative path to the .bbm
        // object. Textures are stored in a subdir with the object's
        // name. Here we strip the path and the .bbm extension.
        std::string::size_type n = filename.rfind('/');
        ret->name_ = filename.substr(n+1, filename.size() - n - 5);
        
        ret->load(s, NULL);
        
        return ret.release();
        
    } catch (Exception & e)
    {
        std::string n = "Node::loadFromFile(" + filename + ")";
        e.addHistory(n);
        throw e;
    }    
}


//------------------------------------------------------------------------------
void Node::saveToFile(const std::string & full_name)
{
    serializer::Serializer s(full_name, serializer::SOM_WRITE | serializer::SOM_COMPRESS);

    save(s, Matrix(true));
}


//------------------------------------------------------------------------------
BBM_CREATOR Node::getCreator() const
{
    return creator_;
}



//------------------------------------------------------------------------------
void Node::loadChildren(serializer::Serializer & s,
                        const Matrix * inverse_parent_transform)
{
    uint32_t num_children;
    s.get(num_children);

    for (unsigned i=0; i<num_children; ++i)
    {
        uint32_t node_type;
        s.get(node_type);

        Node * cur_child;
        switch(node_type)
        {
        case NT_MESH:
            cur_child = new MeshNode();
            break;
        case NT_GROUP:
            cur_child = new GroupNode();
            break;
        case NT_EFFECT:
            cur_child = new EffectNode();
            break;
        default:
            Exception e("Bad Node Type: ");
            e << (void*)node_type;
            throw e;
        }
        
        cur_child->load(s,inverse_parent_transform);
        
        child_.push_back(cur_child);
    }
}

//------------------------------------------------------------------------------
void Node::saveChildren(serializer::Serializer & s)
{
    s.put((uint32_t)child_.size());
    for (unsigned c=0; c<child_.size(); ++c)
    {
        child_[c]->save(s, Matrix(true));
    }
}


//------------------------------------------------------------------------------
/**
 *  The stored Blender transforms are total transforms, but we need
 *  transforms relative to our parent, so we combine the transform
 *  with inverse_parent_transform.
 *
 *  In order to be able to work with orthonormal matrices only, all
 *  scaling factors are extracted from the model transformation
 *  matrices and are directly applied to the vertex data.
 *
 *  The matrix stored for a child object by blender is the total
 *  transform to put it into its position. We need the transform
 *  relative to its parent object, so we pass along the inverse parent
 *  transform and use it to calculate that relative transform.
 *
 *  \param s The serializer to read from.
 *
 *  \param scale Will be set to the extracted scaling factors.
 *
 *  \param inverse_parent_transform The inverse of the parent's total
 *  transform. NULL for top-level objects.
 *
 */
void Node::loadTransform(serializer::Serializer & s,
                         Matrix & scale,
                         const Matrix * inverse_parent_transform)
{
    Matrix total_transform_with_scaling;
    
    s.get(total_transform_with_scaling);
    matExtractScaleFactors(&total_transform_, &scale, &total_transform_with_scaling);
    
    if (inverse_parent_transform)
    {
        transform_ = *inverse_parent_transform * total_transform_;
    } else
    {
        transform_ = total_transform_;
    }
}

//------------------------------------------------------------------------------
void Node::loadGroups(serializer::Serializer & s)
{
    std::string group_string;
    s.get(group_string);
    Tokenizer tok(group_string, ';');
    while(!tok.isEmpty())
    {
        std::vector<std::string> disj_term;
        Tokenizer tok2(tok.getNextWord(), ',');
        while(!tok2.isEmpty())
        {
            disj_term.push_back(tok2.getNextWord());
        }
        groups_.push_back(disj_term);
    }
}


//------------------------------------------------------------------------------
void Node::saveGroups(serializer::Serializer & s)
{
    std::string group_string;
    
    for (unsigned i=0; i<groups_.size(); ++i)
    {
        for (unsigned j=0; j<groups_[i].size(); ++j)
        {
            group_string += groups_[i][j];
            if (j != groups_[i].size()-1) group_string += ',';
        }
        if (i != groups_.size()-1) group_string += ';';
    }
    
    s.put(group_string);
}


//------------------------------------------------------------------------------
void Node::doAccept(NodeVisitor & visitor)
{
    visitor.apply(this);
}


//------------------------------------------------------------------------------
MeshNode::MeshNode()
{
}

//------------------------------------------------------------------------------
MeshNode::~MeshNode()
{
    for (unsigned i=0; i<mesh_.size(); ++i)
    {
        delete mesh_[i];
    }
}




//------------------------------------------------------------------------------
void MeshNode::load(serializer::Serializer & s,
                    const Matrix * inverse_parent_transform)
{    
    s.get(name_);

    s_log << Log::debug('r') << "Importing mesh " << name_ << ".\n";

    loadGroups(s);

    Matrix scale;
    loadTransform(s, scale, inverse_parent_transform);
    
    uint32_t num_meshes;
    s.get(num_meshes);

    try
    {
        mesh_.resize(num_meshes);
    } catch (std::exception & std_e)
    {
        Exception e(std_e.what());
        e.addHistory("MeshNode::load(");
        e << name_ << ")";
        throw e;
    }

    // Transforms at this level are directly applied to the vertex
    // data in order to save performance.
    for (unsigned m=0; m<num_meshes; ++m)
    {
        mesh_[m] = new Mesh(s, scale);
        mesh_[m]->transform(transform_);
    }
    
    // Now we must correct total_transform_ and transform_
    // accordingly.
    //
    // Mesh Nodes Can only have additional mesh nodes as children, so
    // keep total_transform_ constant across the mesh-only hierarchy
    // (At the parent group's transform). This is achieved by undoing
    // our local transform.
    total_transform_ *= transform_.getAffineInverse();

    // Was baked into vertex data.
    transform_.loadIdentity();

    Matrix inverse_transform = total_transform_.get3x3Inverse();
    loadChildren(s, &inverse_transform);
}


//------------------------------------------------------------------------------
void MeshNode::save(serializer::Serializer & s,
                    const Matrix parent_transform)
{
    s.put((uint32_t)NT_MESH);
    s.put(name_);

    saveGroups(s);

    s.put(total_transform_);

    s.put((uint32_t)mesh_.size());
    
    for (unsigned m=0; m<mesh_.size(); ++m)
    {
        mesh_[m]->save(s);
    }

    saveChildren(s);
}


//------------------------------------------------------------------------------
const std::vector<Mesh*> & MeshNode::getMeshes() const
{
    return mesh_;
}


//------------------------------------------------------------------------------
void MeshNode::doAccept(NodeVisitor & visitor)
{
    visitor.apply(this);
}



//------------------------------------------------------------------------------
GroupNode::GroupNode()
{
}

//------------------------------------------------------------------------------
GroupNode::~GroupNode()
{
}


//------------------------------------------------------------------------------
/**
 *  \see MeshNode::load
 */
void GroupNode::load(serializer::Serializer & s,
                     const Matrix * inverse_parent_transform)
{
    s.get(name_);

    loadGroups(s);    

    Matrix scale;
    loadTransform(s, scale, inverse_parent_transform);    
    
    Matrix inverse_transform = total_transform_.get3x3Inverse();
    loadChildren(s, &inverse_transform);
}

//------------------------------------------------------------------------------
void GroupNode::save(serializer::Serializer & s,
                     const Matrix parent_transform)
{
    s.put((uint32_t)NT_GROUP);

    s.put(name_);

    saveGroups(s);    

    s.put(total_transform_);
    
    saveChildren(s);
}


//------------------------------------------------------------------------------
void GroupNode::doAccept(NodeVisitor & visitor)
{
    visitor.apply(this);
}



//------------------------------------------------------------------------------
EffectNode::EffectNode()
{
}


//------------------------------------------------------------------------------
EffectNode::~EffectNode()
{
}

//------------------------------------------------------------------------------
void EffectNode::load(serializer::Serializer & s,
                      const Matrix * inverse_parent_transform)
{
    s.get(name_);

    loadGroups(s);
    
    Matrix m;
    loadTransform(s, m, inverse_parent_transform);
}


//------------------------------------------------------------------------------
void EffectNode::save(serializer::Serializer & s,
                      const Matrix parent_transform)
{
    s.put((uint32_t)NT_EFFECT);


    s.put(name_);

    saveGroups(s);

    s.put(total_transform_);
}



//------------------------------------------------------------------------------
void EffectNode::doAccept(NodeVisitor & visitor)
{
    visitor.apply(this);
}


//------------------------------------------------------------------------------
NodeVisitor::NodeVisitor()
{
}

//------------------------------------------------------------------------------
NodeVisitor::~NodeVisitor()
{
}

    

//------------------------------------------------------------------------------
Mesh::Mesh(serializer::Serializer & s, const Matrix & scale) :
    flags_(0),
    parallax_strength_(0),
    normal_strength_(0)
{
    // Get mesh-specific flags
    s.get(flags_);

    s.get(shader_);
    
    // Get vertex data
    s.get(vertex_data_);
    s.get(normal_data_);
    s.get(tangent_data_);
    s.get(bi_tangent_data_);
    s.get(tex_data_);    
    s.get(index_data_);

    assert(!(flags_ & BMO_LIGHT_MAP) || tex_data_.size() == 2);
    assert(index_data_.size() % 3 == 0);
    assert(vertex_data_.size() != 0);
    assert(index_data_.size()  != 0);


    // Get texture names
    s.get(texture_name_);
    if (flags_ & BMO_LIGHT_MAP)    s.get(lm_name_);
    if (flags_ & BMO_EMISSIVE_MAP) s.get(em_name_);
    
    if (flags_ & bbm::BMO_BUMP_MAP)
    {
        s.get(parallax_strength_);
        s.get(normal_strength_);
    }

    s.get(material_.specular_.r_);
    material_.specular_.g_ = material_.specular_.b_ = material_.specular_.r_;
    s.get(material_.shininess_);



    
    // transform all vertices and build AABB
    for (std::vector<Vector>::iterator it = vertex_data_.begin();
         it != vertex_data_.end();
         ++it)
    {
        *it = scale.transformPoint(*it);        
    }

    // transform all normals with the inverse scale matrix and
    // renormalize
    Matrix inv_scale(true);
    inv_scale.scale(1.0f / scale._11,
                    1.0f / scale._22,
                    1.0f / scale._33);
    for (std::vector<Vector>::iterator it = normal_data_.begin();
         it != normal_data_.end();
         ++it)
    {
        *it = inv_scale.transformVector(*it);
        it->safeNormalize();
    }    
}

//------------------------------------------------------------------------------
Mesh::~Mesh()
{
}

//------------------------------------------------------------------------------
void Mesh::save(serializer::Serializer & s)
{
    s.put(flags_);

    s.put(shader_);

    s.put(vertex_data_);
    s.put(normal_data_);
    s.put(tangent_data_);
    s.put(bi_tangent_data_);
    s.put(tex_data_);    
    s.put(index_data_);


    s.put(texture_name_);
    if (flags_ & BMO_LIGHT_MAP)    s.put(lm_name_);
    if (flags_ & BMO_EMISSIVE_MAP) s.put(em_name_);
    
    if (flags_ & bbm::BMO_BUMP_MAP)
    {
        s.put(parallax_strength_);
        s.put(normal_strength_);
    }

    s.put(material_.specular_.r_);
    s.put(material_.shininess_);
}


//------------------------------------------------------------------------------
/**
 *  Tranforms the mesh's normals and vertices with the specified
 *  transform.
 *
 *  \param mat An affine matrix.
 */
void Mesh::transform(const Matrix & mat)
{
    for (unsigned v=0; v<vertex_data_.size(); ++v)
    {
        vertex_data_[v] = mat.transformPoint(vertex_data_[v]);
    }

    // There are no scaling factors...
//     mat.invert3x3();
//     mat = mat.getTranspose3x3();

    for (unsigned n=0; n<normal_data_.size(); ++n)
    {
        normal_data_[n] = mat.transformVector(normal_data_[n]);
    }
}

    
//------------------------------------------------------------------------------
const Material & Mesh::getMaterial() const
{
    return material_;
}


//------------------------------------------------------------------------------
const std::string & Mesh::getShader() const
{
    return shader_;
}


//------------------------------------------------------------------------------
const std::string & Mesh::getTextureName() const
{
    return texture_name_;
}

//------------------------------------------------------------------------------
const std::string & Mesh::getLmName() const
{
    return lm_name_;
}

//------------------------------------------------------------------------------
const std::string & Mesh::getEmName() const
{
    return em_name_;
}



//------------------------------------------------------------------------------
/**
 *  First array: texcoord sets
 *  Second array: texcoords from one set
 */
const std::vector<std::vector<TexCoord> > & Mesh::getTexData() const
{
    return tex_data_;
}

//------------------------------------------------------------------------------
const std::vector<Vector>   & Mesh::getNormalData() const
{
    return normal_data_;
}

//------------------------------------------------------------------------------
const std::vector<Vector>   & Mesh::getTangentData() const
{
    return tangent_data_;
}

//------------------------------------------------------------------------------
const std::vector<Vector>   & Mesh::getBiTangentData() const
{
    return bi_tangent_data_;
}

//------------------------------------------------------------------------------
const std::vector<Vector>   & Mesh::getVertexData() const
{
    return vertex_data_;
}

//------------------------------------------------------------------------------
const std::vector<uint16_t> & Mesh::getIndexData() const
{
    return index_data_;
}


//------------------------------------------------------------------------------
std::vector<std::vector<TexCoord> > & Mesh::getTexData()
{
	return tex_data_;
}



//------------------------------------------------------------------------------
std::vector<Vector>   & Mesh::getNormalData()
{
	return normal_data_;
}


//------------------------------------------------------------------------------
std::vector<Vector>   & Mesh::getTangentData()
{
    return tangent_data_;
}

//------------------------------------------------------------------------------
std::vector<Vector>   & Mesh::getBiTangentData()
{
    return bi_tangent_data_;
}


//------------------------------------------------------------------------------
std::vector<Vector>   & Mesh::getVertexData()
{
	return vertex_data_;
}

//------------------------------------------------------------------------------
std::vector<uint16_t> & Mesh::getIndexData()
{
	return index_data_;
}



//------------------------------------------------------------------------------
uint16_t Mesh::getFlags() const
{
    return flags_;
}


//------------------------------------------------------------------------------
float Mesh::getParallaxStrength() const
{
    return parallax_strength_;
}

//------------------------------------------------------------------------------
float Mesh::getNormalStrength() const
{
    return normal_strength_;
}


//------------------------------------------------------------------------------
void Mesh::generateTangentSpaceInfo()
{
    if (!tangent_data_.empty()) return;
    s_log << Log::warning
          << "generating TS info\n";
    
    assert(vertex_data_.size());
    assert(normal_data_.size() == vertex_data_.size());
    assert(tex_data_.size());
    assert(tex_data_[0].size() == vertex_data_.size());
    
    std::vector<MeshMender::Vertex> mender_vertex(vertex_data_.size());
    for (unsigned i=0; i<mender_vertex.size(); ++i)
    {
        mender_vertex[i].pos = D3DXVECTOR3(vertex_data_[i].x_,
                                           vertex_data_[i].y_,
                                           vertex_data_[i].z_);
        mender_vertex[i].normal = D3DXVECTOR3(normal_data_[i].x_,
                                              normal_data_[i].y_,
                                              normal_data_[i].z_);
        mender_vertex[i].s = tex_data_[0][i].tu_;
        mender_vertex[i].t = tex_data_[0][i].tv_;
    }

    std::vector<unsigned int>       mender_index (index_data_.size());    
    for (unsigned i=0; i<mender_index.size(); ++i)
    {
        mender_index[i] = index_data_[i];
    }
    
    std::vector<unsigned int> mapping;
    
    MeshMender mender;
    mender.Mend(mender_vertex,
                mender_index,
                mapping,
                0.0f, 0.0f, 0.0f,
                1.0f,
                MeshMender::DONT_CALCULATE_NORMALS,
                MeshMender::RESPECT_SPLITS,
                MeshMender::DONT_FIX_CYLINDRICAL);


    // Mesh mender will increase the total vertex count.
    vertex_data_    .resize(mender_vertex.size());
    normal_data_    .resize(mender_vertex.size());
    tex_data_[0]    .resize(mender_vertex.size());
    tangent_data_   .resize(mender_vertex.size());
    bi_tangent_data_.resize(mender_vertex.size());    

    for(unsigned c=0;c < mender_vertex.size(); c++)
    {
        vertex_data_[c] = Vector(mender_vertex[c].pos.x,
                                 mender_vertex[c].pos.y,
                                 mender_vertex[c].pos.z);

        tex_data_[0][c].tu_ = mender_vertex[c].s;
        tex_data_[0][c].tv_ = mender_vertex[c].t;
        
        Vector n (mender_vertex[c].normal.x,
                  mender_vertex[c].normal.y,
                  mender_vertex[c].normal.z);
        Vector t (mender_vertex[c].tangent.x,
                  mender_vertex[c].tangent.y,
                  mender_vertex[c].tangent.z);
        Vector bt(mender_vertex[c].binormal.x,
                  mender_vertex[c].binormal.y,
                  mender_vertex[c].binormal.z);

        // MeshMender doesn't return orthonormal axes....
        Matrix m;
        m.loadOrientation(n,t);
        // We don't know about handedness of tangent space system
        if (vecDot(&bt, &m.getX()) < 0.0f) m.getX() = -m.getX();

        normal_data_    [c] = -m.getZ();
        tangent_data_   [c] =  m.getY();
        bi_tangent_data_[c] =  m.getX();
    }

    // Copy new vertex indices
    assert(mender_index .size() == index_data_.size());
    for (unsigned i=0; i < mender_index.size(); ++i)
    {
        index_data_[i] = mender_index[i];
    }

    // Update remaining texcoord sets to new vertex order
    for (unsigned t=1; t<tex_data_.size(); ++t)
    {
        std::vector<TexCoord> new_texcoord_data(mender_vertex.size());

        for (unsigned i=0; i<new_texcoord_data.size(); ++i)
        {
            new_texcoord_data[i] = tex_data_[t][mapping[i]];
        }
        
        tex_data_[t].swap(new_texcoord_data);        
    }
}


//------------------------------------------------------------------------------
std::string Mesh::getUniqueMaterialIdentifier(uint16_t flag_mask) const
{
    return (shader_ +
            texture_name_ + lm_name_ + em_name_ +
            toString(flags_ & flag_mask) +
            toString(parallax_strength_) +
            toString(normal_strength_) );
}

} // namespace importer


