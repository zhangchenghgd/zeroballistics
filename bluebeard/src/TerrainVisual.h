

#include <vector>

#include <osg/NodeCallback>
#include <osg/Geode>


#include "Matrix.h"

namespace bbm
{
    class DetailTexInfo;
}


namespace terrain
{
    
class InstancePlacer;
class ClipmapGrid;
class TerrainDataClient;

const int COLORMAP_UNIT          = 0; 
const int COLORMAP_NEXT_LVL_UNIT = 1; 
//const int SHADOW_TEX_UNIT        = 2; // same as scenemanager
const int DETAIL_TEX0_UNIT        = 3; // All detail tex units follow this one
 
const unsigned HEIGHT_NEXT_LVL_ATTRIB       = 1;
const unsigned DETAIL_COEFF_ATTRIB          = 2; 
const unsigned DETAIL_COEFF_NEXT_LVL_ATTRIB = 3; 
 

//------------------------------------------------------------------------------
class TerrainVisual : public osg::NodeCallback
{
 public:
    TerrainVisual();
    virtual ~TerrainVisual();
    
    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);


    void setData(const TerrainDataClient * terrain_data);

    void setTextures(const std::vector<bbm::DetailTexInfo> & tex_info,
                     const std::string & level_name);

    void removeFromScene();
    
 protected:

    void reset();

    osg::Geode * geode_; ///< We are update callback of geode, so
                         ///avoid cyclic dependency by not using
                         ///ref_ptr
    
    std::vector<osg::ref_ptr<ClipmapGrid> > grid_;

    std::vector<InstancePlacer*> instance_placer_;
};


} // namespace terrain 
