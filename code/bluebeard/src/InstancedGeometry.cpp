
#include "InstancedGeometry.h"


#include <osg/MatrixTransform>

#include <loki/static_check.h>

#include "Log.h"
#include "UtilsOsg.h"
#include "Profiler.h"
#include "RigidBody.h"
#include "SceneManager.h"
#include "BbmOsgConverter.h"
#include "LodUpdater.h"
#include "ParameterManager.h"

#undef min
#undef max

const unsigned INSTANCE_TEXTURE_INDEX = 2;

const char * INSTANCE_DATA_UNIFORM_NAME = "instance_data";

osg::ref_ptr<osg::Drawable::Extensions> InstancedPrimitive::drawable_extensions_;
osg::ref_ptr<osg::GL2Extensions>        InstancedPrimitive::gl2_extensions_;

//------------------------------------------------------------------------------
InstanceProxy::InstanceProxy(InstancedGeometryDescription * desc) :
    desc_(desc),
    cur_lod_lvl_(NUM_LOD_LVLS)
{
    desc_->addInstance(this, InstanceData());
}


//------------------------------------------------------------------------------
InstanceProxy::~InstanceProxy()
{
    assert(desc_);
    desc_->removeInstance(this);
}


//------------------------------------------------------------------------------
void InstanceProxy::setLodLevel(unsigned lvl)
{
    assert(lvl <= NUM_LOD_LVLS);
    assert(desc_);
    
    if (lvl == cur_lod_lvl_) return;

    InstanceData data = desc_->removeInstance(this);
    cur_lod_lvl_ = lvl;
    desc_->addInstance(this, data);
}

//------------------------------------------------------------------------------
unsigned InstanceProxy::getLodLevel() const
{
    return cur_lod_lvl_;
}

//------------------------------------------------------------------------------
const std::vector<float> & InstanceProxy::getLodDists() const
{
    static std::vector<float> def(NUM_LOD_LVLS, 0.0f);
    
    if (desc_) return desc_->getLodDists();
    else return def;
}


//------------------------------------------------------------------------------
void InstanceProxy::setLodDists(const std::vector<float> & dists)
{
    assert(false);
}


//------------------------------------------------------------------------------
void InstanceProxy::setTransform(const Matrix & mat)
{
    assert(desc_);    

    InstanceData & data = desc_->getInstanceData(this);

//     // XXXX currently all instanced objects are scaled by a random
//     // amount
//     data.scale_ = s_params.get<float>("instances.min_scale") +
//         (float)rand()/RAND_MAX*(s_params.get<float>("instances.max_scale") -
//                                 s_params.get<float>("instances.min_scale"));
    
    data.alpha_ = atan2f(mat._13, mat._11);

    data.pos_ = mat.getTranslation();

    desc_->dirtyBound(cur_lod_lvl_);
}

//------------------------------------------------------------------------------
void InstanceProxy::addToScene()
{
    assert(desc_);

    setLodLevel(NUM_LOD_LVLS-1);

    s_lod_updater.addLodNode(this);
}


//------------------------------------------------------------------------------
void InstanceProxy::removeFromScene()
{
    assert(desc_);

    setLodLevel(NUM_LOD_LVLS);
    
    s_lod_updater.removeLodNode(this);
}


//------------------------------------------------------------------------------
Vector InstanceProxy::getPosition() const
{
    return desc_->getInstanceData(this).pos_;
}

//------------------------------------------------------------------------------
void InstanceProxy::setPosition(const Vector & v)
{
    desc_->getInstanceData(this).pos_ = v;
}


//------------------------------------------------------------------------------
float InstanceProxy::getRadius() const
{
    return desc_->getRadius();
}


//------------------------------------------------------------------------------
osg::MatrixTransform * InstanceProxy::getOsgNode()
{
    return NULL;
}


//------------------------------------------------------------------------------
void InstanceProxy::setDiffuse(float d)
{
    desc_->getInstanceData(this).diffuse_ = d;
}



//------------------------------------------------------------------------------
InstancedGeometryDescription * InstanceProxy::getDescription() const
{
    return desc_;
}

//------------------------------------------------------------------------------
void InstancingInfo::addInstance(const InstanceProxy * proxy, const InstanceData & instance_data)
{
    assert(!hasInstance(proxy));
    
    position_map_[proxy] = instance_data_.size();
    instance_data_.push_back(instance_data);
    proxy_.push_back(proxy);

    for (unsigned i=0; i<primitive_.size(); ++i)
    {
        primitive_[i]->dirtyBound();
    }
}



//------------------------------------------------------------------------------
InstanceData InstancingInfo::removeInstance(const InstanceProxy * proxy)
{
    PositionMap::iterator pos = position_map_.find(proxy);
    assert(pos != position_map_.end());

    unsigned index = pos->second;
    assert(index < instance_data_.size());
    InstanceData ret = instance_data_[index];    

    if (index != instance_data_.size() -1)
    {
        // Move the last entry in our instance_data_ array to the now void
        // position.
        instance_data_[index] = instance_data_.back();
        proxy_        [index] = proxy_.        back();

        PositionMap::iterator last_pos = position_map_.find(proxy_.back());
        assert(last_pos->second == instance_data_.size()-1);
        last_pos->second = index;
    }
    
    proxy_.pop_back();
    instance_data_.pop_back();
    position_map_.erase(pos);    

    for (unsigned i=0; i<primitive_.size(); ++i)
    {
        primitive_[i]->dirtyBound();
    }

    return ret;
}

//------------------------------------------------------------------------------
bool InstancingInfo::hasInstance(const InstanceProxy * proxy) const
{
    return position_map_.find(proxy) != position_map_.end();
}

//------------------------------------------------------------------------------
InstanceData & InstancingInfo::getInstanceData(const InstanceProxy * proxy)
{
    assert(hasInstance(proxy));

    return instance_data_[position_map_[proxy]];
}


//------------------------------------------------------------------------------
InstancedPrimitive::InstancedPrimitive(const osg::Geometry & geometry, const InstancingInfo & lod_info) :
    osg::Geometry(geometry),
    instance_radius_(geometry.computeBound().radius()),
    info_(lod_info),
    instance_data_uniform_location_(-1),
    instance_batch_size_(s_params.get<unsigned>("client.graphics.instance_batch_size"))
{
    LOKI_STATIC_CHECK(sizeof(InstanceData) % sizeof(osg::Vec3) == 0, size_mismatch);
    
    if (!drawable_extensions_)
    {
        drawable_extensions_ = osg::Drawable::getExtensions(s_scene_manager.getContextId(), true);
        gl2_extensions_      = osg::GL2Extensions::Get(s_scene_manager.getContextId(), true);
    }

    replicateGeometry(geometry);


    
    getStateSet()->addUniform(new osg::Uniform(osg::Uniform::FLOAT_VEC3,
                                               INSTANCE_DATA_UNIFORM_NAME,
                                               NUM_VEC3S_PER_INSTANCE*instance_batch_size_));
}


//------------------------------------------------------------------------------
InstancedPrimitive::~InstancedPrimitive()
{
}

    
//------------------------------------------------------------------------------
osg::BoundingBox InstancedPrimitive::computeBound() const
{
    osg::BoundingBox ret;

    for (InstanceDataContainer::const_iterator it = info_.instance_data_.begin();
         it != info_.instance_data_.end();
         ++it)
    {
        ret.expandBy(it->pos_.x_,
                     it->pos_.y_,
                     it->pos_.z_);
    }

    ret._min -= osg::Vec3(instance_radius_, instance_radius_, instance_radius_)*2;
    ret._max += osg::Vec3(instance_radius_, instance_radius_, instance_radius_)*2;
    
    return ret;
}


//------------------------------------------------------------------------------
/**
 *  Replicate just enough of Geometry::drawImplementation for our
 *  purposes to set up the state for rendering pseudo instanced
 *  objects.
 */
void InstancedPrimitive::drawImplementation(osg::RenderInfo& renderInfo) const
{
#ifdef ENABLE_DEV_FEATURES
    ADD_STATIC_CONSOLE_VAR(bool, render_instances, true);
    if (!render_instances) return;
#endif
    
    PROFILE(InstancedPrimitive::drawImplementation);

    if (info_.instance_data_.empty()) return;
    if (instance_batch_size_ == 0) return;
    
    osg::State & state = *renderInfo.getState();

    if (instance_data_uniform_location_ == -1)
    {
        if (!getStateSet() || !getStateSet()->getAttribute(osg::StateAttribute::PROGRAM))
        {
            // This might happen if graphic settings were changed :-(
#ifdef ENABLE_DEV_FEATURES
            assert(false);
#endif
            return;
        }
        
        osg::Program * program = (osg::Program*)getStateSet()->getAttribute(osg::StateAttribute::PROGRAM);

        osg::Program::PerContextProgram * pcp = program->getPCP(state.getContextID());

        // Workaround for ATI graphic cards: need to append [0] to the
        // uniform name for arrays....
        instance_data_uniform_location_ = pcp->getUniformLocation(INSTANCE_DATA_UNIFORM_NAME);
        if (instance_data_uniform_location_ == -1)
        {
            instance_data_uniform_location_ =
                pcp->getUniformLocation((std::string(INSTANCE_DATA_UNIFORM_NAME) + "[0]").c_str());
        }

        if (instance_data_uniform_location_ == -1)
        {
            s_log << Log::error
                  << "Error getting array uniform in InstancedPrimitive::drawImplementation. "
                  << "Disabling instanced objects.\n";
            instance_batch_size_ = 0;
            return;
        }
    }

    

    unsigned int unit;
    for(unit=0;unit < _texCoordList.size();++unit)
    {
        state.setTexCoordPointer(unit, _texCoordList[unit].array.get());
    }
    state.disableTexCoordPointersAboveAndIncluding(unit);

    state.setVertexPointer(_vertexData.array.get());
    if (_normalData.array.valid()) state.setNormalPointer(_normalData.array.get());
    

    // Tangent & Bitangent
    unsigned int index;
    for( index = 0; index < _vertexAttribList.size(); ++index )
    {
        const osg::Array* array = _vertexAttribList[index].array.get();
        state.setVertexAttribPointer(index, array, _vertexAttribList[index].normalize);

    }
    state.disableVertexAttribPointersAboveAndIncluding( index );

    
    assert(_primitives.size() == 1);



    draw_elements_->bindBuffer(state);





    unsigned num_full_batches = info_.instance_data_.size() / instance_batch_size_;
    for (unsigned batch=0; batch < num_full_batches; ++batch)
    {
        gl2_extensions_->glUniform3fv(instance_data_uniform_location_,
                                      instance_batch_size_ * NUM_VEC3S_PER_INSTANCE,
                                      (GLfloat*)&info_.instance_data_[batch*instance_batch_size_]);
        draw_elements_->drawCapped(state, 0);        
    }

    unsigned num_remaining_elements = info_.instance_data_.size() - num_full_batches*instance_batch_size_;
    if (num_remaining_elements)
    {
        gl2_extensions_->glUniform3fv(instance_data_uniform_location_,
                                      num_remaining_elements*NUM_VEC3S_PER_INSTANCE,
                                      (GLfloat*)&info_.instance_data_[num_full_batches*instance_batch_size_]);
        draw_elements_->drawCapped(state, draw_elements_->size() / instance_batch_size_ * num_remaining_elements);
    }

    state.unbindVertexBufferObject();
    state.unbindElementBufferObject();
}

//------------------------------------------------------------------------------
/**
 *   This never seems to be called... Implemented anyway, perhaps this
 *   is used in a later version of osg?
 */
bool InstancedPrimitive::supports (const osg::PrimitiveFunctor &) const
{
    s_log << "InstancedPrimitive::supports() called\n";
    return true;
}


//------------------------------------------------------------------------------
/**
 *  Currently, this is only a rough estimate.
 */
void InstancedPrimitive::accept (osg::PrimitiveFunctor & pf) const
{
    if (instance_batch_size_ == 0) return;
    
    for (unsigned i=0; i< info_.instance_data_.size()/instance_batch_size_; ++i)
    {
        Geometry::accept(pf);
    }
}


//------------------------------------------------------------------------------
void InstancedPrimitive::replicateGeometry(const osg::Geometry & geometry)
{
    // First determine possible number of batches based on uniform count (XXX hardcoded for now...)



    // Now replicate the geometry and add index information to each
    // and every vertex.
    assert(geometry.getVertexArray()   &&
           geometry.getVertexArray()   ->getType() == osg::Array::Vec3ArrayType);
    assert(geometry.getNormalArray()   &&
           geometry.getNormalArray()   ->getType() == osg::Array::Vec3ArrayType);
    assert(geometry.getTexCoordArray(0) &&
           geometry.getTexCoordArray(0)->getType() == osg::Array::Vec2ArrayType);
    assert(geometry.getNumPrimitiveSets() == 1 &&
           geometry.getPrimitiveSet(0)->getType() == osg::PrimitiveSet::DrawElementsUShortPrimitiveType);    


    
    unsigned num_vertices = geometry.getVertexArray()->getNumElements();
    unsigned total_num_vertices = instance_batch_size_ * num_vertices;
    assert(geometry.getNormalArray()   ->getNumElements() == num_vertices);
    assert(geometry.getTexCoordArray(0)->getNumElements() == num_vertices);
    assert(dynamic_cast<const osg::DrawElementsUShort*>(geometry.getPrimitiveSet(0)));

    const osg::DrawElementsUShort & src_draw_elements =
        *(dynamic_cast<const osg::DrawElementsUShort*>(geometry.getPrimitiveSet(0)));
    draw_elements_ = new DrawElementsUIntCapped(src_draw_elements.getMode(),
                                                src_draw_elements.size()*instance_batch_size_);

    for (unsigned b=0; b<instance_batch_size_; ++b)
    {
        for (unsigned i=0; i<src_draw_elements.size(); ++i)
        {
            (*draw_elements_)[b*src_draw_elements.size()+i] = b*num_vertices + (unsigned)src_draw_elements[i];
        }
    }

    setPrimitiveSet(0, draw_elements_.get());
 

    osg::FloatArray & instance_index_array = *(new osg::FloatArray(total_num_vertices));
    for (unsigned i=0; i<total_num_vertices; ++i)
    {
        instance_index_array[i] = i / num_vertices;
    }
    setTexCoordArray(INSTANCE_TEXTURE_INDEX, &instance_index_array);
    

    // No need to set bindings as we draw this ourselves...
    setVertexArray(     replicateArray<osg::Vec3Array>(geometry.getVertexArray(),    instance_batch_size_));
    setNormalArray(     replicateArray<osg::Vec3Array>(geometry.getNormalArray(),    instance_batch_size_));
    setTexCoordArray(0, replicateArray<osg::Vec2Array>(geometry.getTexCoordArray(0), instance_batch_size_));
    
    
    if (geometry.getVertexAttribArray(TANGENT_ATTRIB))
    {
        assert(geometry.getVertexAttribArray(BI_TANGENT_ATTRIB));

        setVertexAttribArray(TANGENT_ATTRIB,
                             replicateArray<osg::Vec3Array>(geometry.getVertexAttribArray(TANGENT_ATTRIB),
                                                            instance_batch_size_));
        setVertexAttribArray(BI_TANGENT_ATTRIB,
                             replicateArray<osg::Vec3Array>(geometry.getVertexAttribArray(BI_TANGENT_ATTRIB),
                                                            instance_batch_size_));        
    }
}




//------------------------------------------------------------------------------
InstancedPrimitive::DrawElementsUIntCapped::DrawElementsUIntCapped(GLenum mode, unsigned no) :
    osg::DrawElementsUInt(mode, no)
{
}
    
//------------------------------------------------------------------------------
void InstancedPrimitive::DrawElementsUIntCapped::bindBuffer(osg::State& state)
{
    const osg::ElementBufferObject* ebo = getElementBufferObject();
    state.bindElementBufferObject(ebo);   
}
    
//------------------------------------------------------------------------------
void InstancedPrimitive::DrawElementsUIntCapped::drawCapped(osg::State& state, unsigned num_indices)
{
    glDrawElements(_mode,
                   num_indices == 0 ? size() : num_indices,
                   GL_UNSIGNED_INT,
                   getElementBufferObjectOffset());
}




//------------------------------------------------------------------------------
InstancedGeometryDescription::InstancedGeometryDescription(const std::string & name) :
    name_(name),
    radius_(0.0f),
    lod_dist_(NUM_LOD_LVLS, 0.0f)
{
    geode_ = new osg::Geode;
    geode_->setName("Instance Geode for " + name_);

    RootNodeUserData * ud = new RootNodeUserData;
    ud->flags_ |= bbm::BNO_SHADOW_RECEIVER | bbm::BNO_INSTANCED;
    geode_->setUserData(ud);
    
    s_scene_manager.addNode(geode_.get());

    lod_info_.reserve(NUM_LOD_LVLS); // Neccessary because we pass references to elements of this array!
}


//------------------------------------------------------------------------------
InstancedGeometryDescription::~InstancedGeometryDescription()
{
    if (geode_->referenceCount() > 1)
    {
        DeleteNodeVisitor d(geode_.get());
        s_scene_manager.getRootNode()->accept(d);
    }
    
    assert(geode_->referenceCount() == 1);
}

//------------------------------------------------------------------------------
void InstancedGeometryDescription::addGeometry(const osg::Geometry * geometry, unsigned lod)
{
    lod_info_.resize(std::max(lod_info_.size(), lod+1));

    InstancingInfo & cur_info = lod_info_[lod];
    cur_info.primitive_.push_back(new InstancedPrimitive(*geometry, cur_info));
    
    geode_->addDrawable(cur_info.primitive_.back().get());
    radius_ = std::max(radius_, geometry->getBound().radius());    
}


//------------------------------------------------------------------------------
float InstancedGeometryDescription::getRadius() const
{
    return radius_;
}

//------------------------------------------------------------------------------
void InstancedGeometryDescription::setLodDists(const std::vector<float> & dists)
{
    lod_dist_ = dists;
}


//------------------------------------------------------------------------------
const std::vector<float> & InstancedGeometryDescription::getLodDists() const
{
    return lod_dist_;
}


//------------------------------------------------------------------------------
void InstancedGeometryDescription::dirtyBound(unsigned lod)
{
    if (lod == NUM_LOD_LVLS) return;

    assert(lod<lod_info_.size());

    for (unsigned i=0; i<lod_info_[lod].primitive_.size(); ++i)
    {
        lod_info_[lod].primitive_[i]->dirtyBound();
    }
}
    
//------------------------------------------------------------------------------
void InstancedGeometryDescription::addInstance(const InstanceProxy * proxy,
                                               const InstanceData & instance_data)
{
    unsigned lod = proxy->getLodLevel();
    
    if (lod == NUM_LOD_LVLS)
    {
        unused_info_.addInstance(proxy, instance_data);
    } else
    {
        assert(!lod_info_.empty());

        if (lod >= lod_info_.size())
        {
            s_log << Log::warning
                  << "not all LOD levels available for instanced object "
                  << name_
                  <<"\n";
            lod = lod_info_.size() -1;
        }
    
        lod_info_[lod].addInstance(proxy, instance_data);
    }
}


//------------------------------------------------------------------------------
InstanceData InstancedGeometryDescription::removeInstance(const InstanceProxy * proxy)
{
    unsigned lod_level = proxy->getLodLevel();

    if (lod_level == NUM_LOD_LVLS)
    {
        return unused_info_.removeInstance(proxy);
    } else
    {
        assert(!lod_info_.empty());

        lod_level = std::min(lod_info_.size()-1, lod_level);

        return lod_info_[lod_level].removeInstance(proxy);
    }
}


//------------------------------------------------------------------------------
bool InstancedGeometryDescription::hasInstance(const InstanceProxy * proxy) const
{
    for (unsigned l=0; l<lod_info_.size(); ++l)
    {
        if (lod_info_[l].hasInstance(proxy)) return true;
    }
    
    return unused_info_.hasInstance(proxy);
}


//------------------------------------------------------------------------------
InstanceData & InstancedGeometryDescription::getInstanceData(const InstanceProxy * proxy)
{
    unsigned lod_level = proxy->getLodLevel();

    if (lod_level == NUM_LOD_LVLS)
    {
        return unused_info_.getInstanceData(proxy);
    } else
    {
        assert(!lod_info_.empty());

        lod_level = std::min(lod_info_.size()-1, lod_level);
    
        return lod_info_[lod_level].getInstanceData(proxy);
    }
}



//------------------------------------------------------------------------------
osg::Geode * InstancedGeometryDescription::getGeode()
{
    return geode_.get();
}



//------------------------------------------------------------------------------
/**
 *  Creates a description structure for instanced geometry from the specified osg node.
 *
 *  \param name The name of the model. Instance descriptions are stored under this name.
 *
 *  \param node The osg node to create the description from. Must only
 *  have geodes as children. 
 */
InstancedGeometryDescription * InstanceManager::getOrCreateInstanceDescription(const std::string & name,
                                                                               const osg::MatrixTransform * node)
{
    std::map<std::string, InstancedGeometryDescription*>::iterator it = desc_.find(name);

    if (it != desc_.end())
    {
        return it->second;
    } else
    {
        assert(node->getNumChildren() != 0);
        
        InstancedGeometryDescription * desc = new InstancedGeometryDescription(name);        
        desc_[name] = desc;


        // retrieve lod bias from model
        const RootNodeUserData * ud = dynamic_cast<const RootNodeUserData*>(node->getUserData());
        assert(ud);
        desc->setLodDists(ud->lod_dist_);
        
        for (unsigned c=0; c<node->getNumChildren(); ++c)
        {
            const osg::Geode * src_geode = dynamic_cast<const osg::Geode*>(node->getChild(c));
            if (!src_geode)
            {
                Exception e("Model ");
                e << name << " is instanced but hasn't got a geode child.\n";
                throw e;
            }
            assert(src_geode->getNumDrawables());

            
            const NodeUserData * user_data = dynamic_cast<const NodeUserData*>(src_geode->getUserData());
            assert(user_data);

            // Now see which lod level this geode is assigned to. If
            // it isn't assigned to any level, assign it to all
            // levels.
            bool found_lod = false;
            for (unsigned l=0; l<NUM_LOD_LVLS; ++l)
            {
                if (user_data->isGroupMember(LOD_LVL_NAME[l]))
                {
                    found_lod = true;
                    addToDescription(desc, src_geode, l);
                }
            }
            
            if (!found_lod)
            {
                // No lod level specified, assign to all lod levels 1.
//                for (unsigned l=0; l<NUM_LOD_LVLS; ++l)
                {
                    addToDescription(desc, src_geode, 0);
                }
            }
        }

        return desc;
    }
}

//------------------------------------------------------------------------------
void InstanceManager::reset()
{
    s_log << Log::debug('d')
          << "InstanceManager::reset()\n";
    
    for (std::map<std::string, InstancedGeometryDescription*>::iterator it = desc_.begin();
         it != desc_.end();
         ++it)
    {
        delete it->second;
    }

    desc_.clear();
}


//------------------------------------------------------------------------------
/**
 *  Convenience function. Adds all drawables (which must be
 *  geometries) from geode to the specified instance description at
 *  the given lod level.
 */
void InstanceManager::addToDescription(InstancedGeometryDescription * desc,
                                       const osg::Geode * geode,
                                       unsigned lod_lvl) const
{
    for (unsigned d=0; d<geode->getNumDrawables(); ++d)
    {
        const osg::Geometry * geometry = dynamic_cast<const osg::Geometry*>(geode->getDrawable(d));
        assert(geometry);
        desc->addGeometry(geometry, lod_lvl);
    }
}
