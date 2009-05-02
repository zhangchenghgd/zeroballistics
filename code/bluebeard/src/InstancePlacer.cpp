

#include "InstancePlacer.h"

#include <limits>

#include <loki/static_check.h>

#include "ParameterManager.h"
#include "ReaderWriterBbm.h"
#include "TerrainDataClient.h"
#include "LodUpdater.h"
#include "Scheduler.h"
#include "SceneManager.h"
#include "LevelData.h"

#undef min
#undef max

namespace terrain
{


std::auto_ptr<InstancedGeometryDescription> PlacedInstance::dummy_desc_;
    
    
//------------------------------------------------------------------------------
PlacedInstance::PlacedInstance() :
    InstanceProxy(dummy_desc_.get())
{
}

//------------------------------------------------------------------------------
PlacedInstance::PlacedInstance(const PlacedInstance & other) :
    InstanceProxy(other)
{
    desc_->addInstance(this, desc_->getInstanceData(&other));
}


//------------------------------------------------------------------------------
PlacedInstance::~PlacedInstance()
{
}

//------------------------------------------------------------------------------
void PlacedInstance::addToScene()
{
    assert(desc_);
    if (desc_ == dummy_desc_.get()) return;
    
    setLodLevel(0);
}


//------------------------------------------------------------------------------
void PlacedInstance::removeFromScene()
{
    assert(desc_);
    if (desc_ == dummy_desc_.get()) return;

    setLodLevel(NUM_LOD_LVLS);
}


//------------------------------------------------------------------------------
/**
 *  With this function it is possible to reuse this instance proxy for
 *  a different object.
 */
void PlacedInstance::assignToDescription(InstancedGeometryDescription * desc)
{
    assert(desc_);

    if (desc == NULL) desc = dummy_desc_.get();
    
    if (desc_ == desc) return;
    
    InstanceData data = desc_->removeInstance(this);
    desc_ = desc;

    if (desc_ == dummy_desc_.get()) cur_lod_lvl_ = NUM_LOD_LVLS;
    else cur_lod_lvl_ = 0;
    
    desc_->addInstance(this, data);
}


//------------------------------------------------------------------------------
void PlacedInstance::setAlphaAndScale(float a, float s)
{
    assert(desc_);

    InstanceData & data = desc_->getInstanceData(this);

    data.scale_ = s;
    data.alpha_ = a;
}




//------------------------------------------------------------------------------
InstancePlacer::InstancePlacer(unsigned layer_num,
                               const TerrainDataClient * terrain_data,
                               const std::vector<bbm::DetailTexInfo> & tex_info) :
    cur_camera_cell_x_(-1000),
    cur_camera_cell_z_(-1000),
    terrain_(terrain_data),
    instances_per_cell_(0),
    layer_num_(layer_num)
{
    cos_threshold_steepness_ = cosf(deg2Rad(s_params.get<float>("instances.threshold_steepness")));
    unsigned num_layers = s_params.get<unsigned>("instances.num_layers");
    
    if (s_params.get<std::vector<float> >("instances.density_factor").size() != num_layers)
    {
        throw Exception("density factor not specified for all layers.");
    }
    
    cell_resolution_ = s_params.get<unsigned>("instances.cell_resolution");
    cell_size_       = s_params.get<float>   ("instances.base_draw_distance")*(layer_num+1) / cell_resolution_;
    
    if (!PlacedInstance::dummy_desc_.get())
    {
        PlacedInstance::dummy_desc_.reset(new InstancedGeometryDescription("dummy"));
    }
    
    assert(cell_resolution_);
    cell_resolution_ |= 1; // Make sure we always have an odd number of cells

    instance_probability_.resize(tex_info.size());
    instance_prototype_.  resize(tex_info.size());



    


    
    // The maximum density determines the number of instances allocated
    float max_density = std::numeric_limits<float>::min();
    for (unsigned terrain_type=0; terrain_type<tex_info.size(); ++terrain_type)
    {
        max_density = std::max(max_density, tex_info[terrain_type].grass_density_);

        // Copy probabilities...
        instance_probability_[terrain_type].resize(tex_info[terrain_type].zone_info_.size());
        for (unsigned t=0; t < instance_probability_[terrain_type].size(); ++t)
        {
            instance_probability_[terrain_type][t] = tex_info[terrain_type].zone_info_[t].probability_;
        }



        // Load model prototypes
        instance_prototype_[terrain_type].resize(tex_info[terrain_type].zone_info_.size());
        for (unsigned obj=0; obj<instance_prototype_[terrain_type].size(); ++obj)
        {
            osg::ref_ptr<InstanceProxy> proxy;
            try
            {
                proxy = dynamic_cast<InstanceProxy*>(
                    ReaderWriterBbm::loadModel(tex_info[terrain_type].zone_info_[obj].model_).get());
                if (!proxy.get())
                {
                    Exception e("Error loading automatically placed object \"");
                    e << tex_info[terrain_type].zone_info_[obj].model_
                      << "\". Perhaps it's not instanced?";
                    throw e;
                }
            } catch (Exception & e)
            {
                e.addHistory("InstancePlacer::InstancePlacer()");
                throw e;
            }
            
            instance_prototype_[terrain_type][obj] = proxy;
        }
    }                                          


                                          

    // Depending on the layer we are in, we want to have a different
    // instance density. max_density is the maximum density in
    // instances/m^2 specified for any terrain type and determines the
    // number of prototypes we have to allocate per cell. Terrain
    // types with smaller density have some prototypes unused
    // (probability doesn't sum up to one).

    // convert density to instances per cell,
    // adjust for layer number.
    instances_per_cell_ =
        (unsigned)(cell_size_*cell_size_ * max_density *
                   s_params.get<std::vector<float> >("instances.density_factor")[layer_num_]*
                   s_params.get<float>("instances.user_density"));

    // Perhaps there are no instances in this level, or user chose
    // zero density
    if (instances_per_cell_ == 0) return;
    
    
    normalizeProbabilities(instance_probability_, max_density, tex_info);

    
    initCells();

    s_scheduler.addTask(PeriodicTaskCallback(this, &InstancePlacer::update),
                        s_params.get<float>("instances.update_dt"),
                        "InstancePlacer::update",
                        &fp_group_);
}


//------------------------------------------------------------------------------
InstancePlacer::~InstancePlacer()
{
}



//------------------------------------------------------------------------------
void InstancePlacer::update(float dt)
{
    const Vector & pos = s_scene_manager.getCamera().getPos();
    
    int new_camera_cell_x = (int)(pos.x_ / cell_size_);
    int new_camera_cell_z = (int)(pos.z_ / cell_size_);    

    instance_torus_.shiftOrigin(new_camera_cell_x - cur_camera_cell_x_,
                                new_camera_cell_z - cur_camera_cell_z_);
    
    cur_camera_cell_x_ = new_camera_cell_x;
    cur_camera_cell_z_ = new_camera_cell_z;


    // Perform brute force culling for now...
    float half_size = cell_size_*0.5f;
    Vector offset (half_size, 0.1f, half_size);
    for (unsigned r=0; r<cell_resolution_; ++r)
    {
        for (unsigned c=0; c<cell_resolution_; ++c)
        {
            InstanceCell & cur_cell = instance_cell_[c+r*cell_resolution_];
            const Vector & center = cur_cell.center_;
            AABB aabb(center - offset,
                      center + offset);

            if (s_scene_manager.getCamera().getFrustum().intersect(aabb) == CS_OUTSIDE)
            {
                for (unsigned i=0; i<cur_cell.instance_.size(); ++i)
                {
                    cur_cell.instance_[i].removeFromScene();
                }
            } else
            {
                for (unsigned i=0; i<cur_cell.instance_.size(); ++i)
                {
                    cur_cell.instance_[i].addToScene();
                }
            }
        }
    }
}



//------------------------------------------------------------------------------
void InstancePlacer::initCells()
{
    instance_torus_.setSize(cell_resolution_, cell_resolution_);
    instance_torus_.setPos(cur_camera_cell_x_,
                           cur_camera_cell_z_);
    instance_torus_.setBufferFillCallback(BufferFillCallback(this, &InstancePlacer::fillInstanceCell));
    
    instance_cell_.resize(cell_resolution_*
                          cell_resolution_);

    for (unsigned r=0; r<cell_resolution_; ++r)
    {
        for (unsigned c=0; c<cell_resolution_; ++c)
        {
            // Pre-calculate random positions for instance proxies
            InstanceCell & cur_cell = instance_cell_[c + r*cell_resolution_];
            cur_cell.instance_.resize(instances_per_cell_);

            for (unsigned m=0; m<instances_per_cell_; ++m)
            {
                float scale = s_params.get<float>("instances.min_scale") +
                    (float)rand()/RAND_MAX*(s_params.get<float>("instances.max_scale") -
                                            s_params.get<float>("instances.min_scale"));

                float rand_angle = normalizeAngle((float)rand()/RAND_MAX);

                        
                cur_cell.instance_[m].setPosition(Vector(((float)rand()/RAND_MAX - 0.5f)*cell_size_,
                                                         0.0f,
                                                         ((float)rand()/RAND_MAX - 0.5f)*cell_size_));
                
                // encode multiple of base draw distance in rotation
                // angle for shader to find correct draw distance
                cur_cell.instance_[m].setAlphaAndScale(rand_angle + 2*PI*(layer_num_+1), scale);
            }
        }
    }
}


//------------------------------------------------------------------------------
void InstancePlacer::fillInstanceCell(BUFFERFILL_CALLBACK_TYPE type,
                                      unsigned dest_x, unsigned dest_y,
                                      int source_x, int source_y,
                                      unsigned width, unsigned height,
                                      const ToroidalBuffer * buffer)
{
    if (type != BCT_FILL) return;
        
    Vector2d new_cell_center_base(
        (float)source_x*cell_size_ - ((float)(cell_resolution_>>1)-0.5f)*cell_size_,
        (float)source_y*cell_size_ - ((float)(cell_resolution_>>1)-0.5f)*cell_size_);
    
    for (unsigned r=0; r<height; ++r)
    {
        for (unsigned c=0; c<width; ++c)
        {
            InstanceCell & cur_cell = instance_cell_[dest_x+c + (dest_y+r)*cell_resolution_];

            Vector2d new_center = new_cell_center_base + Vector2d(c*cell_size_, r*cell_size_);
            Vector2d offset = new_center - cur_cell.center_;
            cur_cell.center_ = new_center;


            // Get height too
            Vector dummy_n;
            terrain_->getHeightAndNormal(cur_cell.center_.x_,
                                         cur_cell.center_.z_,
                                         cur_cell.center_.y_, dummy_n);

            
            for (unsigned i=0; i<instances_per_cell_; ++i)
            {
                Vector pos = cur_cell.instance_[i].getPosition();
                pos += offset;
                float h;
                Vector n;
                terrain_->getHeightAndNormal(pos.x_, pos.z_, h, n);
                pos.y_ = h;
                
                cur_cell.instance_[i].setPosition(pos);
                cur_cell.instance_[i].setDiffuse(terrain_->getColor(pos.x_, pos.z_).getBrightness());
                
                // Bail if the terrain is too steep.
                if (n.y_ < cos_threshold_steepness_)
                {
                    cur_cell.instance_[i].assignToDescription(NULL);
                    continue;
                }

                unsigned tex_type = terrain_->getPrevalentDetail(pos.x_,
                                                                 pos.z_);
                
                // Bail if no prototype exists for this texture type.
                if (tex_type >= instance_prototype_.size())
                {
                    cur_cell.instance_[i].assignToDescription(NULL);
                    continue;
                }
                std::vector<osg::ref_ptr<InstanceProxy> > & prototypes = instance_prototype_[tex_type];
                if (prototypes.empty())
                {
                    cur_cell.instance_[i].assignToDescription(NULL);
                    continue;
                }
                

                float r = (float)rand() / RAND_MAX;
                float cum_p = 0.0f;
                unsigned p=0;
                do
                {
                    cum_p += instance_probability_[tex_type][p];
                    assert(cum_p <= 1.01f);
                    if (cum_p >= r)
                    {
                        cur_cell.instance_[i].assignToDescription(prototypes[p]->getDescription());
                        break;
                    }
                    ++p;
                } while (p<prototypes.size());
                
                if (p==prototypes.size()) cur_cell.instance_[i].assignToDescription(NULL);
            }
        }
    }
}


//------------------------------------------------------------------------------
/**
 *  Converts the relative probabilities for instance models into
 *  absolute ones, taking into account the instance density of the
 *  underlying terrain type.
 *
 *  
 *
 *  \param instance_probability [in,out] The probability for a model occuring
 *  as specified in Grome. First vector are different terrain types,
 *  second is model type. Doesn't sum up to one, doesn't reflect
 *  different densities of terrain types yet.
 *
 *  \param max_density The maximum instance density of any terrain
 *  type. This will determine the number of actual instance prototypes
 *  allocated per toroidal buffer cell.
 *
 *  \param tex_info Description of the different terrain types.
 */
void InstancePlacer::normalizeProbabilities(std::vector<std::vector<float> > & instance_probability,
                                            float max_density,
                                            const std::vector<bbm::DetailTexInfo> & tex_info) const
{
    assert(instance_probability.size() == tex_info.size());
    
    for (unsigned i=0; i<instance_probability.size(); ++i)
    {
        float sum = 0.0f;
        for (unsigned j=0; j<instance_probability[i].size(); ++j)
        {
            sum += instance_probability[i][j];
        }

        for (unsigned j=0; j<instance_probability[i].size(); ++j)
        {
            float density = tex_info[i].grass_density_;
            if (!equalsZero(sum)) instance_probability[i][j] *= density / (max_density * sum);
        }
    }
}


} // namespace terrain
