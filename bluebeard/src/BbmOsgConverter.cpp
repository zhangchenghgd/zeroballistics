

#include "BbmOsgConverter.h"

#include <sstream>
#include <limits>

#include <osg/Notify>
#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>
#include <osgDB/ReadFile>

#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/PrimitiveSet>
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/TexEnv>
#include <osg/BlendFunc>
#include <osg/AlphaFunc>
#include <osg/CullFace>
#include <osg/Depth>  // for transparent sorting

#include "Log.h"
#include "BbmImporter.h"
#include "UtilsOsg.h"

#include "TextureManager.h"
#include "Paths.h"
#include "SceneManager.h"

#include "EffectManager.h"

#undef min
#undef max

BbmOsgVisitor::StatesetMap BbmOsgVisitor::state_set_cache_;


//------------------------------------------------------------------------------
/**
 *  Clears state sets which are deleted from our stateset cache.
 */
class StatesetCacheCleaner : public osg::Observer
{
    virtual void objectDeleted (void * p)
        {
            for (BbmOsgVisitor::StatesetMap::iterator it = BbmOsgVisitor::state_set_cache_.begin();
                 it != BbmOsgVisitor::state_set_cache_.end();
                 ++it)
            {
                if (it->second == p)
                {
                    BbmOsgVisitor::state_set_cache_.erase(it);
                    return;
                }
            }

            assert(false);
        }
} g_cleaner;



//------------------------------------------------------------------------------
/**
 *  Enable all lod levels per default. This is neccessary so particle
 *  effects with finite life time are triggered by the initial fire()
 *  (in EffectNode copy constructor).
 */
NodeUserData::NodeUserData()
{
    for (unsigned i=0; i<NUM_LOD_LVLS; ++i)
    {
        enabled_groups_.push_back(LOD_LVL_NAME[i]);
    }
}


//------------------------------------------------------------------------------
/**
 *  \return Whether the node status could have changed (the group was
 *  actually added / removed)
 */
bool NodeUserData::enableGroup(const std::string & group, bool e)
{
    std::vector<std::string>::iterator it = std::find(enabled_groups_.begin(), enabled_groups_.end(), group);
    
    if (e)
    {
        if (it != enabled_groups_.end()) return false;
        enabled_groups_.push_back(group);
    } else
    {
        if (it == enabled_groups_.end()) return false;
        enabled_groups_.erase(it);
    }
    return true;
}


//------------------------------------------------------------------------------
/**
 *  Checks enabled groups and group assignments to see whether this
 *  node should be enabled.
 */
bool NodeUserData::isActive() const
{
    for (unsigned conj_term=0; conj_term < groups_.size(); ++conj_term)
    {
        bool r = false;
        for (unsigned disj_term =0; disj_term < groups_[conj_term].size(); ++disj_term)
        {
            if (std::find(enabled_groups_.begin(),
                          enabled_groups_.end(),
                          groups_[conj_term][disj_term]) != enabled_groups_.end())
            {
                r = true;
                break;
            }
        }
        if (!r) return false;
    }
    return true;
}


//------------------------------------------------------------------------------
/**
 *  Returns whether the given group name exists at all in the node's
 *  group conditions.
 */
bool NodeUserData::isGroupMember(const std::string & name) const
{
    for (unsigned conj_term =0; conj_term<groups_.size(); ++conj_term)
    {
        for (unsigned disj_term=0; disj_term<groups_[conj_term].size(); ++disj_term)
        {
            if (groups_[conj_term][disj_term] == name) return true;
        }
    }

    return false;
}


//------------------------------------------------------------------------------
/**
 *  If no lod group is present, insert all lod groups. This is
 *  required so things are culled when beyond last lod level.
 *
 *  lod starts with lod1 in modeler!!
 */
void NodeUserData::addLodsIfNeccessary()
{
    bool has_lod = false;
    for (unsigned i=0; i<NUM_LOD_LVLS; ++i)
    {
        if (isGroupMember(LOD_LVL_NAME[i]))
        {
            has_lod = true;
            break;
        }
    }

    if (!has_lod)
    {
        groups_.resize(groups_.size()+1);
        for (unsigned i=0; i<NUM_LOD_LVLS; ++i)
        {
            groups_.back().push_back(LOD_LVL_NAME[i]);
        }
    }
}


//------------------------------------------------------------------------------
BbmOsgVisitor::BbmOsgVisitor() :
    root_node_flags_mask_(0),
    mesh_flags_mask_(0),
    use_shaders_(true)
{
    // Cancel shadow flags if shadow is disabled
    if (!s_scene_manager.getShadow())
    {
        root_node_flags_mask_ |= bbm::BNO_SHADOW_RECEIVER | bbm::BNO_SHADOW_BLOCKER;
    }

    unsigned quality = s_params.get<unsigned>("client.graphics.shader_quality");
    if (quality < 2)
    {
        mesh_flags_mask_ |= bbm::BMO_BUMP_MAP;
    }

    if (quality < 1)
    {
        mesh_flags_mask_ |= bbm::BMO_PER_PIXEL_LIGHTING;
        mesh_flags_mask_ |= bbm::BMO_EMISSIVE_MAP;

        use_shaders_ = false;
    }

    mesh_flags_mask_      = ~mesh_flags_mask_;
    root_node_flags_mask_ = ~root_node_flags_mask_;
}


//------------------------------------------------------------------------------
BbmOsgVisitor::~BbmOsgVisitor()
{
}
    

//------------------------------------------------------------------------------
void BbmOsgVisitor::apply(bbm::Node* node)
{
    assert(!root_.get() && cur_node_.empty()); // perhaps relaxed later...

    root_ = new osg::MatrixTransform();
    root_->setName(node->getName());
    cur_node_.push(root_.get());
    
    bbm_name_ = node->getName();

    RootNodeUserData * data = new RootNodeUserData();
    data->flags_    = node->getFlags() & root_node_flags_mask_;
    data->lod_dist_ = s_params.get<std::vector<float> >("lod_class." + node->getLodClass());
    root_->setUserData(data);

    root_node_flags_ = data->flags_;
}


//------------------------------------------------------------------------------
void BbmOsgVisitor::apply(bbm::MeshNode* mesh_node)
{
    assert(root_.get());
    unsigned num_meshes = mesh_node->getMeshes().size();
    assert(num_meshes);
    
    osg::Geode * geode = new osg::Geode;
    geode->setName(mesh_node->getName());
    
    if (mesh_node->getNumChildren() != 0)
    {    
        osg::Group * new_node = new osg::Group();
        
        new_node->setName(mesh_node->getName());
        new_node->addChild(geode);

        cur_node_.top()->addChild(new_node);
        cur_node_.push(new_node);    

        createGroupUserData(mesh_node, new_node);
    } else
    {
        cur_node_.top()->addChild(geode);
        cur_node_.push(NULL);

        createGroupUserData(mesh_node, geode);
    }    
    
    for (unsigned m=0; m<num_meshes; ++m)
    {
        addMesh(mesh_node->getMeshes()[m], geode);
    }
}


//------------------------------------------------------------------------------
void BbmOsgVisitor::apply(bbm::GroupNode* group_node)
{
    assert(root_.get());
    assert(!cur_node_.empty());
    
    osg::MatrixTransform * new_node = new osg::MatrixTransform(matGl2Osg(group_node->getTransform()));

    cur_node_.top()->addChild(new_node);
    cur_node_.push(new_node);

    new_node->setName(group_node->getName());

    createGroupUserData(group_node, new_node);
}



//------------------------------------------------------------------------------
void BbmOsgVisitor::apply(bbm::EffectNode* effect)
{
    EffectManager::EffectPair eff = s_effect_manager.createEffect(
        effect->getName(),
        effect->getTransform().getTranslation(),
        -effect->getTransform().getZ(),
        true,
        cur_node_.top());
    
    createGroupUserData(effect, eff.first.get());
    createGroupUserData(effect, eff.second.get());
    
    cur_node_.push(NULL);
}


//------------------------------------------------------------------------------
void BbmOsgVisitor::pop()
{
    cur_node_.pop();
}


//------------------------------------------------------------------------------
osg::Node * BbmOsgVisitor::getOsgRoot() const
{
    return root_.get();
}


//------------------------------------------------------------------------------
void BbmOsgVisitor::addMesh(bbm::Mesh * mesh, osg::Geode * geode)
{
    osg::Geometry * geometry = new osg::Geometry;
    geode->addDrawable(geometry);

    createGeometry(mesh, geometry);

    // set Material and such things ...
    createStateSet(mesh, geometry);

    // use vertex buffer instead of display lists
    geometry->setSupportsDisplayList(false);
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);

    // GeometryUserData is used by createShader to get the
    // properties & shader of the mesh
    GeometryUserData * ud = new GeometryUserData;
    ud->flags_ = getMaskedMeshFlags(mesh);
    if (!mesh->getShader().empty()) ud->shader_ = mesh->getShader();
    geometry->setUserData(ud);

    if (use_shaders_)
    {
        createShader(geometry);
    }
}


//------------------------------------------------------------------------------
/**
 *  Store group conditions found in bbm_node in user data structure
 *  added to osg_node.
 */
void BbmOsgVisitor::createGroupUserData(const bbm::Node * bbm_node, osg::Node * osg_node)
{
    NodeUserData * options = new NodeUserData();

    options->groups_ = bbm_node->getGroups();

    options->addLodsIfNeccessary();
    
    osg_node->setUserData(options);
}


//------------------------------------------------------------------------------
/**
 *  Taking into account mesh_flags_mask_, root_node_flags_ (which have
 *  been masked before), and the mesh material and textures, create a
 *  stateset for the corresponding geometry.
 *
 *  If a stateset already was created for this texture/material
 *  combination in state_set_cache_, reuse it.
 */
void BbmOsgVisitor::createStateSet(const bbm::Mesh * mesh, osg::Geometry * geom)
{
#ifdef ENABLE_DEV_FEATURES
    bool b = false;
    try
    {
        b = s_params.get<bool>("dev.disable_statesets");
    } catch (ParamNotFoundException & e){}
    if (b) return;
#endif


    std::string unique_material_id = mesh->getUniqueMaterialIdentifier(mesh_flags_mask_, root_node_flags_);
                                     
    if (s_params.get<bool>("client.graphics.cache_statesets"))
    {
        StatesetMap::iterator it = state_set_cache_.find(unique_material_id);
        if ((it != state_set_cache_.end()))
        {
            geom->setStateSet(it->second);
            return;
        }
    }
    
    osg::StateSet * state_set = geom->getOrCreateStateSet();

    if (s_params.get<bool>("client.graphics.cache_statesets"))
    {    
        state_set_cache_[unique_material_id] = state_set;
        state_set->addObserver(&g_cleaner);
    }


    state_set->setRenderBinDetails(BN_DEFAULT, "RenderBin");

    if (!(getMaskedMeshFlags(mesh) & bbm::BMO_CULL_FACES))
    {
        state_set->setAttributeAndModes(new osg::CullFace(osg::CullFace::BACK),
                                        osg::StateAttribute::OFF);
    }


    if (use_shaders_)
    {
        state_set->addUniform(new osg::Uniform("specularity", mesh->getMaterial().specular_.r_));
        state_set->addUniform(new osg::Uniform("hardness",    (float)mesh->getMaterial().shininess_));

        if (getMaskedMeshFlags(mesh) & bbm::BMO_BUMP_MAP)
        {
            state_set->addUniform(new osg::Uniform("parallax_strength", mesh->getParallaxStrength()));
            state_set->addUniform(new osg::Uniform("normal_bias",       2.0f - mesh->getNormalStrength()));
        }
    }
    

#ifdef ENABLE_DEV_FEATURES
    bool b2 = false;
    try
    {
        b2 = s_params.get<bool>("dev.disable_texturing");
    } catch (ParamNotFoundException & e){}
    if (!b2)
    {
#endif
        
        // deal with base texture
        Texture * tex = s_texturemanager.getResource(MODEL_TEX_PATH + mesh->getTextureName());
        if (!tex)
        {
            s_log << Log::error
                  << "Missing base texture in "
                  << bbm_name_
                  << "\n";
        } else
        {
            osg::Texture2D * texture_map = tex->getOsgTexture();
            state_set->setTextureAttribute(BASE_TEX_UNIT, texture_map);

            // We need to know the texture size in our bump map shader in
            // order to calculate the normal from the heightmap
            if (getMaskedMeshFlags(mesh) & bbm::BMO_BUMP_MAP)
            {
                if (texture_map->getImage()->s() != texture_map->getImage()->t())
                {
#ifdef ENABLE_DEV_FEATURES
                    s_log << Log::debug('r')
                          << "Bump mapping works properly only with square textures, but "
                          << mesh->getTextureName()
                          << " is "
                          << texture_map->getImage()->s()
                          << "x"
                          << texture_map->getImage()->t()
                          << "\n";
#endif
                }
                assert(use_shaders_);            
                state_set->addUniform(new osg::Uniform("inv_bump_map_size", 1.0f / std::min(texture_map->getImage()->s(),
                                                                                            texture_map->getImage()->t())));
            }
        }

        // deal with LM
        if (getMaskedMeshFlags(mesh) & bbm::BMO_LIGHT_MAP)
        {
            tex = s_texturemanager.getResource(MODEL_TEX_PATH + mesh->getLmName());
            if (!tex)
            {
                s_log << Log::error
                      << "Missing lm texture in "
                      << bbm_name_
                      << "\n";
            } else
            {
                osg::Texture2D * texture_map = tex->getOsgTexture();
                state_set->setTextureAttribute(LIGHT_TEX_UNIT, texture_map);            
            }
        }
    
        if (getMaskedMeshFlags(mesh) & bbm::BMO_EMISSIVE_MAP)
        {
            tex = s_texturemanager.getResource(MODEL_TEX_PATH + mesh->getEmName());
            if (!tex)
            {
                s_log << Log::error
                      << "Missing emissive texture in "
                      << bbm_name_
                      << "\n";
            } else
            {
                osg::Texture2D * texture_map = tex->getOsgTexture();
                state_set->setTextureAttribute(EMISSIVE_TEX_UNIT, texture_map);            
            }
        }
        

        // enable fixed-function alpha testing. for shadows, shaders are
        // disabled and this mechanism takes over.
        if ((getMaskedMeshFlags(mesh) & bbm::BMO_ALPHA_TEST) &&
            !(root_node_flags_ & bbm::BNO_INSTANCED))
        {
            state_set->setAttributeAndModes(new osg::AlphaFunc(osg::AlphaFunc::GEQUAL, 0.5f));
            state_set->setTextureMode(BASE_TEX_UNIT, GL_TEXTURE_2D, osg::StateAttribute::ON);
        }


        // handle fixed function alpha testing, texturing etc for
        // non-shader case
        if (!use_shaders_)
        {
            state_set->setTextureMode(BASE_TEX_UNIT, GL_TEXTURE_2D, osg::StateAttribute::ON);

            if (getMaskedMeshFlags(mesh) & bbm::BMO_ALPHA_TEST)
            {
                state_set->setAttributeAndModes(new osg::AlphaFunc(osg::AlphaFunc::GEQUAL, 0.5f));
            }


            if (getMaskedMeshFlags(mesh) & bbm::BMO_LIGHTING)
            {
                state_set->setMode(GL_LIGHTING, osg::StateAttribute::ON);
            } else
            {
                state_set->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            }

            osg::ref_ptr<osg::Material> material = new osg::Material;

            material->setAmbient  (osg::Material::FRONT_AND_BACK, osg::Vec4(1.0,1.0,1.0,1));
            material->setDiffuse  (osg::Material::FRONT_AND_BACK, osg::Vec4(1.0,1.0,1.0,1));
            material->setSpecular (osg::Material::FRONT_AND_BACK, osg::Vec4(mesh->getMaterial().specular_.r_,
                                                                            mesh->getMaterial().specular_.g_,
                                                                            mesh->getMaterial().specular_.b_,
                                                                            1.0));
            material->setShininess(osg::Material::FRONT_AND_BACK, (float)mesh->getMaterial().shininess_);

            state_set->setAttribute(material.get());            
        }


        
#ifdef ENABLE_DEV_FEATURES
    }
#endif    
    
    // ... and shaders / alpha blending state
    if (getMaskedMeshFlags(mesh) & bbm::BMO_ALPHA_BLEND)
    {            
        state_set->setMode(GL_BLEND, osg::StateAttribute::ON);
        state_set->setAttribute(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        state_set->setRenderBinDetails(BN_TRANSPARENT, "DepthSortedBin");
        // Disable z-buffer writing
        state_set->setAttribute(new osg::Depth(osg::Depth::LESS, 0,1,false));


        // ---------- XXXX No shader -> enable fixed function texturing and material for now ----------
        state_set->setTextureMode(0, GL_TEXTURE_2D, osg::StateAttribute::ON);


        osg::ref_ptr<osg::Material> material = new osg::Material;

        material->setAmbient  (osg::Material::FRONT_AND_BACK, osg::Vec4(1,1,1,1));
        material->setDiffuse  (osg::Material::FRONT_AND_BACK, osg::Vec4(1,1,1,1));
        material->setSpecular (osg::Material::FRONT_AND_BACK, osg::Vec4(0,0,0,0));
        material->setShininess(osg::Material::FRONT_AND_BACK, 0);
        
        state_set->setAttribute(material.get());
    }


}



//------------------------------------------------------------------------------
void BbmOsgVisitor::createGeometry(const bbm::Mesh * mesh, osg::Geometry * geom)
{
    // vertex coords.
    osg::Vec3Array* osg_coords = new osg::Vec3Array(mesh->getVertexData().size());
    for(unsigned c=0;c < mesh->getVertexData().size();c++)
    {
        (*osg_coords)[c].set(mesh->getVertexData()[c].x_,
                             mesh->getVertexData()[c].y_,
                             mesh->getVertexData()[c].z_);
    }
    geom->setVertexArray(osg_coords);



    // create texture coords
    for (unsigned tex_coord_set=0; tex_coord_set < mesh->getTexData().size(); ++tex_coord_set)
    {
        osg::Vec2Array* osg_tcoords = new osg::Vec2Array(mesh->getVertexData().size());
        for(unsigned c=0;c < osg_tcoords->getNumElements(); c++)
        {
            (*osg_tcoords)[c].set(mesh->getTexData()[tex_coord_set][c].tu_,
                                  mesh->getTexData()[tex_coord_set][c].tv_);
        }
        geom->setTexCoordArray(tex_coord_set,osg_tcoords);            
    }
        

    // create normals.
    osg::Vec3Array* osg_normals = new osg::Vec3Array(mesh->getNormalData().size());
    for(unsigned c=0;c < mesh->getNormalData().size();c++)
    {
        (*osg_normals)[c].set(mesh->getNormalData()[c].x_,
                              mesh->getNormalData()[c].y_,
                              mesh->getNormalData()[c].z_);
    }
    
    geom->setNormalArray(osg_normals);
    geom->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);



    // Tangent & bitangent, if in use
    if (getMaskedMeshFlags(mesh) & bbm::BMO_BUMP_MAP)
    {
        assert(mesh->getTangentData().size() == mesh->getNormalData().size());
        
        osg::Vec3Array* osg_tangents    = new osg::Vec3Array(mesh->getNormalData().size());
        osg::Vec3Array* osg_bi_tangents = new osg::Vec3Array(mesh->getNormalData().size());
        for(unsigned c=0;c < mesh->getNormalData().size();c++)
        {
            (*osg_tangents)[c].set(mesh->getTangentData()[c].x_,
                                   mesh->getTangentData()[c].y_,
                                   mesh->getTangentData()[c].z_);
            (*osg_bi_tangents)[c].set(mesh->getBiTangentData()[c].x_,
                                      mesh->getBiTangentData()[c].y_,
                                      mesh->getBiTangentData()[c].z_);
        }
    
        geom->setVertexAttribArray  (TANGENT_ATTRIB,    osg_tangents);
        geom->setVertexAttribArray  (BI_TANGENT_ATTRIB, osg_bi_tangents);
        geom->setVertexAttribBinding(TANGENT_ATTRIB,    osg::Geometry::BIND_PER_VERTEX);
        geom->setVertexAttribBinding(BI_TANGENT_ATTRIB, osg::Geometry::BIND_PER_VERTEX);
    }

    

    // create primitives
    int num_indices = mesh->getIndexData().size();
    osg::DrawElementsUShort* elements = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES,num_indices);
    osg::DrawElementsUShort::iterator index_itr = elements->begin();
        

    for (unsigned c=0;c < mesh->getIndexData().size();c++)
    {
        *index_itr++ = mesh->getIndexData()[c];
    }
       
    geom->addPrimitiveSet(elements);
}


//------------------------------------------------------------------------------
void BbmOsgVisitor::createShader(osg::Geometry * cur_geometry)
{
    GeometryUserData * mesh_ud = dynamic_cast<GeometryUserData*>(cur_geometry->getUserData());
    if (!mesh_ud) return;

    if (mesh_ud->flags_ & bbm::BMO_ALPHA_BLEND) return;

    // If lighting is off, override PP_LIGHTING and BUMP_MAP.
    if (!(mesh_ud->flags_ & bbm::BMO_LIGHTING))
    {
        mesh_ud->flags_ &= ~bbm::BMO_BUMP_MAP;
        mesh_ud->flags_ &= ~bbm::BMO_PER_PIXEL_LIGHTING;
    }

    // bump mapping is a PP technique, so this is always on.
    if (mesh_ud->flags_ & bbm::BMO_BUMP_MAP)
    {
        mesh_ud->flags_ |= bbm::BMO_PER_PIXEL_LIGHTING;
    }
        
    std::string l;
    std::string defines;
    if (root_node_flags_ & bbm::BNO_SHADOW_RECEIVER)  { defines += "#define SHADOW\n";      l += " SHADOW "; }
    if (root_node_flags_ & bbm::BNO_INSTANCED)        { defines += "#define INSTANCED\n";   l += " INSTANCED "; }
        
    if (mesh_ud->flags_ & bbm::BMO_LIGHTING)          { defines += "#define LIGHTING\n";    l += " LIGHTING "; }
    if (mesh_ud->flags_ & bbm::BMO_ALPHA_TEST)        { defines += "#define ALPHA_TEST\n";  l += " ALPHA_TEST "; }
    if (mesh_ud->flags_ & bbm::BMO_PER_PIXEL_LIGHTING){ defines += "#define PP_LIGHTING\n"; l += " PP_LIGHTING ";}


    if (mesh_ud->flags_ & bbm::BMO_LIGHT_MAP)         { defines += "#define LIGHT_MAP\n";   l += " LIGHT_MAP "; }
    if (mesh_ud->flags_ & bbm::BMO_BUMP_MAP)          { defines += "#define BUMP_MAP\n";    l += " BUMP_MAP "; }
    if (mesh_ud->flags_ & bbm::BMO_EMISSIVE_MAP)      { defines += "#define EMISSIVE_MAP\n";l += " EMISSIVE_MAP "; }

    bool reused;
    osg::Program * program = s_scene_manager.getCachedProgram(mesh_ud->shader_, defines, &reused); 
    cur_geometry->getOrCreateStateSet()->setAttribute(program);

    if (!reused && (mesh_ud->flags_ & bbm::BMO_BUMP_MAP))
    {
        program->addBindAttribLocation("tangent",    TANGENT_ATTRIB);
        program->addBindAttribLocation("bi_tangent", BI_TANGENT_ATTRIB);
    }

    if (!reused)
    {
        s_log << Log::debug('r')
              << "created shader with defines "
              << l << "\n";
    }
}


//------------------------------------------------------------------------------
/**
 *  Returns the mesh properties masked for the current quality settings.
 */
uint16_t BbmOsgVisitor::getMaskedMeshFlags(const bbm::Mesh * mesh)
{
    return mesh->getFlags() & mesh_flags_mask_;
}
