

#ifndef BLUEBEARD_INSTANCE_PLACER_INCLUDED
#define BLUEBEARD_INSTANCE_PLACER_INCLUDED





#include "InstancedGeometry.h"
#include "ToroidalBuffer.h"

namespace bbm
{
    class DetailTexInfo;
}


namespace terrain
{



    
class TerrainDataClient;

 
//------------------------------------------------------------------------------
class PlacedInstance : public InstanceProxy
{
 public:
    PlacedInstance();
    PlacedInstance(const PlacedInstance & other);
    
    virtual ~PlacedInstance();

    virtual void addToScene();
    virtual void removeFromScene();
    
    void assignToDescription(InstancedGeometryDescription * desc);

    void setAlphaAndScale(float a, float s);

    static std::auto_ptr<InstancedGeometryDescription> dummy_desc_;
};



//------------------------------------------------------------------------------ 
struct InstanceCell
{
    InstanceCell(): center_(0,0,0) {}
    
    Vector center_;
    std::vector<PlacedInstance> instance_;
};
 

//------------------------------------------------------------------------------
class InstancePlacer
{
 public:
    InstancePlacer(unsigned layer_num,
                   const TerrainDataClient * terrain_data,
                   const std::vector<bbm::DetailTexInfo> & tex_info);
    virtual ~InstancePlacer();

    void update(float dt);

 protected:

    void initCells();

    void fillInstanceCell(BUFFERFILL_CALLBACK_TYPE type,
                          unsigned dest_x, unsigned dest_y,
                          int source_x, int source_y,
                          unsigned width, unsigned height,
                          const ToroidalBuffer * buffer);


    void normalizeProbabilities(std::vector<std::vector<float> > & instance_probability,
                                float max_density,
                                const std::vector<bbm::DetailTexInfo> & tex_info) const;
    
    /// A list of instance prototypes for every terrain type.
    std::vector<std::vector<osg::ref_ptr<InstanceProxy> > > instance_prototype_;
    std::vector<std::vector<float                       > > instance_probability_;

    int cur_camera_cell_x_;
    int cur_camera_cell_z_;
    
    ToroidalBuffer instance_torus_;
    std::vector<InstanceCell> instance_cell_;
        
    const TerrainDataClient * terrain_;

    unsigned cell_resolution_;    ///< Cached param.
    float    cell_size_;          ///< Calculated from params.
    unsigned instances_per_cell_; ///< Calculated from params.

    float cos_threshold_steepness_; ///< Cached param.
    
    unsigned layer_num_;
    
    RegisteredFpGroup fp_group_;
};


} // namespace terrain


#endif
