

#include "TerrainVisual.h"


#include "Log.h"
#include "UtilsOsg.h"
#include "ClipmapGrid.h"
#include "SceneManager.h"
#include "ParameterManager.h"
#include "TextureManager.h"
#include "Shadow.h"
#include "TerrainDataClient.h"
#include "Paths.h"
#include "InstancePlacer.h"
#include "LevelData.h"
#include "BbmOsgConverter.h"

namespace terrain
{

//------------------------------------------------------------------------------
TerrainVisual::TerrainVisual() :
    geode_(NULL)
{
    s_log << Log::debug('i')
          << "TerrainVisual constructor\n";
}


//------------------------------------------------------------------------------
TerrainVisual::~TerrainVisual()
{
    for (unsigned l=0; l<instance_placer_.size(); ++l)
    {
        delete instance_placer_[l];
    }
    s_log << Log::debug('d')
          << "TerrainVisual destructor\n";
    reset();
}
    
//------------------------------------------------------------------------------
void TerrainVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{
    for (unsigned l=0; l<grid_.size(); ++l)
    {
        // Updating has to start with innermost grid because of
        // stitching triangles.
        grid_[l]->update();
    }
}


//------------------------------------------------------------------------------
void TerrainVisual::setData(const TerrainDataClient * terrain_data)
{
    reset();


    // Set the far plane of our camera to encompass the whole terrain
    assert(terrain_data->getResX() == terrain_data->getResZ());
    float new_yon = terrain_data->getHorzScale() * terrain_data->getResX() * sqrtf(2.0f);
    s_scene_manager.getCamera().setYon(new_yon);    
    
    ClipmapGrid::setTerrainData(terrain_data);
    
    if (!terrain_data) return;

    geode_ = new osg::Geode;
    
    geode_->setName("TerrainVisual");
    geode_->setUpdateCallback(this);

    RootNodeUserData * ud = new RootNodeUserData;
    ud->flags_ |= bbm::BNO_SHADOW_RECEIVER;
    geode_->setUserData(ud);
    s_scene_manager.addNode(geode_);
 
    geode_->getOrCreateStateSet()->addUniform(new osg::Uniform("colormap", COLORMAP_UNIT));
    geode_->getOrCreateStateSet()->addUniform(new osg::Uniform("colormap_next_lvl", COLORMAP_NEXT_LVL_UNIT));
    
    unsigned num_grids  = s_params.get<unsigned>("terrain.num_grids");
    unsigned start_grid = s_params.get<unsigned>("terrain.start_grid");
    
    for (unsigned l=0; l<num_grids; ++l)
    {
        grid_.push_back(new ClipmapGrid(start_grid+l, l != 0 ? grid_[l-1].get() : NULL));

        if (l) grid_[l-1]->setParent(grid_.back().get());

        geode_->addDrawable(grid_.back().get());
    }
}

//------------------------------------------------------------------------------
void TerrainVisual::setTextures(const std::vector<bbm::DetailTexInfo> & tex_info,
                                const std::string & lvl_name)
{
    assert(geode_);
    osg::StateSet * state_set = geode_->getStateSet();
    assert(state_set);

    // Clear old values...
    for (unsigned i=0; i<8; ++i)
    {
        state_set->setTextureAttribute(DETAIL_TEX0_UNIT+i, NULL);
        state_set->removeUniform("tex_mat"    + toString(i));
        state_set->removeUniform("detail_tex" + toString(i));
    }
    
    for (unsigned tex_num=0; tex_num<tex_info.size(); ++tex_num)
    {
        // Texture matrix uniform, convert from 4x4 to osg 3x3
        Matrix tex_mat = tex_info[tex_num].matrix_;
        osg::Matrix3 m(tex_mat._11, tex_mat._21, tex_mat._31,
                       tex_mat._12, tex_mat._22, tex_mat._32,
                       tex_mat._13, tex_mat._23, tex_mat._33);
        state_set->addUniform(new osg::Uniform(("tex_mat" + toString(tex_num)).c_str(), m));

        // Texture unit uniform
        state_set->addUniform(new osg::Uniform(("detail_tex" + toString(tex_num)).c_str(),
                                               (int)(DETAIL_TEX0_UNIT + tex_num)));
        
        Texture * texture = s_texturemanager.getResource(LEVEL_PATH + 
                                                         lvl_name   + "/" +
                                                         tex_info[tex_num].name_);
        if (!texture)
        {
            s_log << Log::error
                  << "Can't load terrain detail tex "
                  << tex_info[tex_num].name_
                  << "\n";
            continue;
        }
        state_set->setTextureAttribute(DETAIL_TEX0_UNIT + tex_num,
                                       texture->getOsgTexture());
    }



    // Delete any old instance placers...
    for (unsigned l=0; l<instance_placer_.size(); ++l)
    {
        delete instance_placer_[l];
    }
    try
    {
        unsigned num_layers = s_params.get<unsigned>("instances.num_layers");
        instance_placer_.resize(num_layers);
        for (unsigned l=0; l < num_layers; ++l)
        {
            instance_placer_[l] = new InstancePlacer(l,ClipmapGrid::getTerrainData(), tex_info);
        }
    } catch (Exception & e)
    {
        s_log << Log::error
              << e
              << "\n";
    }    
}


//------------------------------------------------------------------------------
void TerrainVisual::removeFromScene()
{
    if (!geode_) return;
    
    geode_->setUpdateCallback(NULL);

    DeleteNodeVisitor v(geode_);
    s_scene_manager.getRootNode()->accept(v);
}



//------------------------------------------------------------------------------
void TerrainVisual::reset()
{
    if (s_scene_manager.getRootNode())
    {
        DeleteNodeVisitor dn(geode_);
        s_scene_manager.getRootNode()->accept(dn);
    }
    geode_ = NULL;

    grid_.clear();
}

    
} // namespace terrain
