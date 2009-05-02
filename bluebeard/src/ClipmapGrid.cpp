
#include "ClipmapGrid.h"

#include <limits>
#include <cmath>
#include <sstream>

#include "Log.h"
#include "TerrainDataClient.h"
#include "TerrainVisual.h"
#include "ParameterManager.h"
#include "SceneManager.h"
#include "Camera.h"

#undef min
#undef max


namespace terrain
{


//------------------------------------------------------------------------------
struct GridVertex
{
    Vector pos_;
    float32_t height_next_lvl_;
    uint32_t detail_coeff_;
    uint32_t detail_coeff_next_lvl_;
};


int                        ClipmapGrid::resolution_      = 0;    

unsigned                   ClipmapGrid::size_horzpart_x_ = 0; 
unsigned                   ClipmapGrid::size_horzpart_z_ = 0;
unsigned                   ClipmapGrid::size_vertpart_x_ = 0;
unsigned                   ClipmapGrid::size_vertpart_z_ = 0;

unsigned                   ClipmapGrid::clip_gridpoint_[4][4];
    
std::vector<unsigned char> ClipmapGrid::tmp_buffer_;

const TerrainDataClient *  ClipmapGrid::terrain_data_   = NULL;


//------------------------------------------------------------------------------
ClipmapGrid::ClipmapGrid(unsigned level, ClipmapGrid * child) :
    clip_region_x_(0x01000000), // arbitrary high multiple of
                                // (1<<numlevels) to enforce a setpos
                                // on first toroidal shift
    clip_region_z_(0x01000000), 
    vertices_changed_(true),
    vb_(GL_ARRAY_BUFFER_ARB),
    ib_(GL_ELEMENT_ARRAY_BUFFER_ARB),
    child_(child), parent_(NULL),
    level_(level)
{
    if (!resolution_)
    {
        resolution_ = (1<<(s_params.get<unsigned>("terrain.grid_size")+3))-1;

        size_horzpart_x_ = resolution_ - 1;
        size_horzpart_z_ = (resolution_ - 3) >>2;
        size_vertpart_x_ = (resolution_ - 3) >>2;
        size_vertpart_z_ = (resolution_ + 1) >>1;


        // upper horizontal
        clip_gridpoint_[0][0] = 0;
        clip_gridpoint_[0][1] = size_horzpart_z_;
        clip_gridpoint_[0][2] = resolution_-1;
        clip_gridpoint_[0][3] = size_horzpart_z_;

        // lower horizontal
        clip_gridpoint_[1][0] = resolution_-1;
        clip_gridpoint_[1][1] = resolution_-1-size_horzpart_z_;
        clip_gridpoint_[1][2] = 0;
        clip_gridpoint_[1][3] = resolution_-1-size_horzpart_z_;

        // left vertical
        clip_gridpoint_[2][0] = size_vertpart_x_;
        clip_gridpoint_[2][1] = resolution_-1 - size_horzpart_z_;
        clip_gridpoint_[2][2] = size_vertpart_x_;
        clip_gridpoint_[2][3] = size_horzpart_z_;

        // right vertical
        clip_gridpoint_[3][0] = resolution_-1 - size_vertpart_x_;
        clip_gridpoint_[3][1] = size_horzpart_z_;
        clip_gridpoint_[3][2] = resolution_-1 - size_vertpart_x_;
        clip_gridpoint_[3][3] = resolution_-1 - size_horzpart_z_;
    }
    unsigned lightmap_res = (resolution_ +1)*terrain_data_->getLmTexelsPerQuad();

    // Used for IB, VB and Lightmap updates
    tmp_buffer_.resize(lightmap_res*lightmap_res*3);
    

    // -------------------- OSG Stuff --------------------
    setUseDisplayList(false);

    
    std::string defines;
    if (level == s_params.get<unsigned>("terrain.start_grid") &&
        s_scene_manager.getShadow())
    {
        defines = "#define SHADOW\n";
    } else if (level == s_params.get<unsigned>("terrain.num_grids")-1+s_params.get<unsigned>("terrain.start_grid"))
    {
        defines = "#define OUTERMOST_LEVEL\n";
    }
    

    osg::Program * program = s_scene_manager.getCachedProgram("terrain_morph", defines);    
    program->addBindAttribLocation("next_lvl_height",       HEIGHT_NEXT_LVL_ATTRIB);
    program->addBindAttribLocation("detail_coeff",          DETAIL_COEFF_ATTRIB);
    program->addBindAttribLocation("detail_coeff_next_lvl", DETAIL_COEFF_NEXT_LVL_ATTRIB);
    getOrCreateStateSet()->setAttribute(program);
    
    
    color_map_ = new osg::Texture2D();
    color_map_->setTextureSize(lightmap_res, lightmap_res);
    color_map_->setInternalFormat(GL_RGB);

    color_map_->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR);    
    color_map_->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::LINEAR);    
    color_map_->setWrap(osg::Texture2D::WRAP_S,osg::Texture2D::REPEAT);
    color_map_->setWrap(osg::Texture2D::WRAP_T,osg::Texture2D::REPEAT);

    getOrCreateStateSet()->setTextureAttribute(COLORMAP_UNIT, color_map_.get());
    

    offset_uniform_         = new osg::Uniform("offset",         osg::Vec2(0,0));
    offset_parent_uniform_  = new osg::Uniform("offset_parent",  osg::Vec2(0,0));
    getStateSet()->addUniform(offset_uniform_.get());
    getStateSet()->addUniform(offset_parent_uniform_.get());

    // The size of the area covered by the color map in world coordinates.
    getStateSet()->addUniform(new osg::Uniform("inv_colormap_extents",
                                               1.0f / (((resolution_+1) << level_) * terrain_data_->getHorzScale())));



    // To use the full possible range for morphing, we want to
    // construct a linear ramp from 0 at a to 1 at b. c is a point in
    // between the extremes specified by morph_region_percentage.

    // a is the greatest possible distance to the border of the next
    // lower level.
    float a = (((resolution_+1)>>2)<<level_) * terrain_data_->getHorzScale();
    // b is the shortest possible distance to this level's border.
    float b = (((resolution_-3)>>1)<<level_) * terrain_data_->getHorzScale();

    float c = b + s_params.get<float>("terrain.morph_region_percentage")*(a-b);

    float morph_k = 1.0f/(b-c);
    float morph_d = -morph_k*c;
    
    getStateSet()->addUniform(new osg::Uniform("morph_k", morph_k));
    getStateSet()->addUniform(new osg::Uniform("morph_d", morph_d));

    getStateSet()->setRenderBinDetails(BN_TERRAIN, "RenderBin");
    
    setupToroidalBuffers();   
}


//------------------------------------------------------------------------------
ClipmapGrid::~ClipmapGrid()
{
    s_log << Log::debug('d') << "ClipmapGrid destructor\n";
}


//------------------------------------------------------------------------------
osg::BoundingBox ClipmapGrid::computeBound() const
{
    return osg::BoundingBox(clip_region_x_ * terrain_data_->getHorzScale(),
                            terrain_data_->getMinHeight(),
                            clip_region_z_ * terrain_data_->getHorzScale(),
                            (clip_region_x_+((resolution_-1)<<level_)) * terrain_data_->getHorzScale(),
                            terrain_data_->getMaxHeight(),
                            (clip_region_z_+((resolution_-1)<<level_)) * terrain_data_->getHorzScale());
}


//------------------------------------------------------------------------------
void ClipmapGrid::drawImplementation(osg::RenderInfo& renderInfo) const
{
#ifdef ENABLE_DEV_FEATURES
    ADD_STATIC_CONSOLE_VAR(bool, render_terrain, true);
    if (!render_terrain) return;
#endif

    
    vb_.bind();
    GridVertex dummy;

    renderInfo.getState()->disableAllVertexArrays(); // Hmmm.. seems to be left enabled by somebody :-(
    renderInfo.getState()->setVertexPointer(3, GL_FLOAT, sizeof(GridVertex), BUFFER_OFFSET(0));

    renderInfo.getState()->setVertexAttribPointer(HEIGHT_NEXT_LVL_ATTRIB, 1, GL_FLOAT, GL_FALSE,
                                                  sizeof(GridVertex),
                                                  BUFFER_OFFSET((char*)&dummy.height_next_lvl_ - (char*)&dummy));

    renderInfo.getState()->setVertexAttribPointer(DETAIL_COEFF_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_FALSE,
                                                  sizeof(GridVertex),
                                                  BUFFER_OFFSET((char*)&dummy.detail_coeff_ - (char*)&dummy));
    renderInfo.getState()->setVertexAttribPointer(DETAIL_COEFF_NEXT_LVL_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_FALSE,
                                                  sizeof(GridVertex),
                                                  BUFFER_OFFSET((char*)&dummy.detail_coeff_next_lvl_ - (char*)&dummy));


    ib_.bind();
    if (level_ == s_params.get<unsigned>("terrain.start_grid"))
    {
        // Always draw full square at innermost level
        glDrawElements(GL_TRIANGLES, (resolution_-1)*(resolution_-1)*6, GL_UNSIGNED_SHORT, 0);
    } else
    {
        int num_elements;
        for (unsigned i=0; i<4; ++i)
        {
            if (clipRegion(clip_gridpoint_[i]))
            {
                num_elements = 6* (quad_offset_[i+1] - quad_offset_[i]);
                glDrawElements(GL_TRIANGLES, num_elements, GL_UNSIGNED_SHORT,
                               BUFFER_OFFSET(6*quad_offset_[i]*sizeof(uint16_t)));
            }
        }
    }
    ib_.unbind();
    
    glDrawElements(GL_TRIANGLES, index_stitching_tris_.size(), GL_UNSIGNED_SHORT, &index_stitching_tris_[0]);

    renderInfo.getState()->disableVertexAttribPointer(HEIGHT_NEXT_LVL_ATTRIB);
    renderInfo.getState()->disableVertexAttribPointer(DETAIL_COEFF_ATTRIB);
    renderInfo.getState()->disableVertexAttribPointer(DETAIL_COEFF_NEXT_LVL_ATTRIB);
    renderInfo.getState()->disableVertexPointer();


    vb_.unbind();
}


//------------------------------------------------------------------------------
/**
 *   This never seems to be called... Implemented anyway, perhaps this
 *   is used in a later version of osg?
 */
bool ClipmapGrid::supports (const osg::PrimitiveFunctor &) const
{
    s_log << "ClipmapGrid::supports() called\n";
    return true;
}

//------------------------------------------------------------------------------
void ClipmapGrid::accept (osg::PrimitiveFunctor & pf) const
{
    if (level_ == s_params.get<unsigned>("terrain.start_grid"))
    {
        // Always draw full square at innermost level
        pf.drawElements(GL_TRIANGLES, (resolution_-1)*(resolution_-1)*6, (GLushort*)0);
    } else
    {
        int num_elements;
        for (unsigned i=0; i<4; ++i)
        {
            if (clipRegion(clip_gridpoint_[i]))
            {
                num_elements = 6* (quad_offset_[i+1] - quad_offset_[i]);
                pf.drawElements(GL_TRIANGLES, num_elements, (GLushort*)0);
            }
        }
    }
    
    pf.drawElements(GL_TRIANGLES, index_stitching_tris_.size(), (GLushort*)0);
}



//------------------------------------------------------------------------------
void ClipmapGrid::setParent(ClipmapGrid * parent)
{
    parent_ = parent;

    getStateSet()->setTextureAttribute(COLORMAP_NEXT_LVL_UNIT,
                                       parent_->color_map_.get());    
}



//------------------------------------------------------------------------------
void ClipmapGrid::update()
{
    const Vector & camera_pos = s_scene_manager.getCamera().getPos();
    
    vertices_changed_ = false;

    // First calculate the new clip region

    // The area in which the clip region is the same is of size
    // 2^(level+1) vertices.
    unsigned mask = ~((1<<(level_+1))-1);
    
    int new_clip_region_x = (int)floor(camera_pos.x_ / terrain_data_->getHorzScale()) & mask;
    int new_clip_region_z = (int)floor(camera_pos.z_ / terrain_data_->getHorzScale()) & mask;

    // Now determine top-left corner of clip region
    new_clip_region_x -= ((resolution_-3) >> 1) << level_ ;
    new_clip_region_z -= ((resolution_-3) >> 1) << level_ ;
        
    // If active region hasn't changed, no VB upload is necessary.
    if (new_clip_region_x != clip_region_x_ ||
        new_clip_region_z != clip_region_z_)
    {
        vertices_changed_ = true;        

        int dx = (new_clip_region_x - clip_region_x_) >> level_;
        int dz = (new_clip_region_z - clip_region_z_) >> level_;
                
        vb_torus_.       shiftOrigin(dx, dz);
        colormap_torus_. shiftOrigin(dx, dz);
        ib_torus_[0].    shiftOrigin(dx, dz);
        if (child_)
        {
            ib_torus_[1].shiftOrigin(dx, dz);
            ib_torus_[2].shiftOrigin(dx, dz);
            ib_torus_[3].shiftOrigin(dx, dz);
        }
        
        // Update our clip region origin
        clip_region_x_ = new_clip_region_x;
        clip_region_z_ = new_clip_region_z;

        updateOffsetUniforms();
        if (child_) child_->updateOffsetUniforms();

        dirtyBound();
    }

    if (vertices_changed_ || (child_ && child_->vertices_changed_))
    {
        updateStitchingIndices();
    }
}




//------------------------------------------------------------------------------
void ClipmapGrid::getClipRegion(int & x, int & z) const
{
    x = clip_region_x_;
    z = clip_region_z_;
}

//------------------------------------------------------------------------------
void ClipmapGrid::setTerrainData(const TerrainDataClient * terrain_data)
{
    terrain_data_ = terrain_data;
}

//------------------------------------------------------------------------------
const TerrainDataClient * ClipmapGrid::getTerrainData()
{
    return terrain_data_;
}


//------------------------------------------------------------------------------
void ClipmapGrid::fillVertexBuffer(BUFFERFILL_CALLBACK_TYPE type,
                                   unsigned dest_x, unsigned dest_z,
                                   int source_x, int source_z,
                                   unsigned width, unsigned height,
                                   const ToroidalBuffer * buffer)
{
    if (type != BCT_FILL) return;
    
    unsigned dest = (dest_x + dest_z*resolution_)*sizeof(GridVertex);
    
    for (int hz=source_z; hz < source_z+(int)height; ++hz)
    {
        GridVertex * cur_vertex = (GridVertex*)&tmp_buffer_[0];
        assert(tmp_buffer_.size() >= width*sizeof(GridVertex));
        
        for (int hx=source_x; hx < source_x+(int)width; ++hx)
        {
            float h = terrain_data_->getHeightAtGrid(hx, hz, level_);
         
            cur_vertex->pos_ = Vector((hx<<level_)*terrain_data_->getHorzScale(),
                                      h,
                                      (hz<<level_)*terrain_data_->getHorzScale());
            cur_vertex->height_next_lvl_ =
                terrain_data_->getHeightAtGridInterpol(hx,hz, level_);

            
            cur_vertex->detail_coeff_          = terrain_data_->getDetailAtGrid        (hx, hz, level_);
            cur_vertex->detail_coeff_next_lvl_ = terrain_data_->getDetailAtGridInterpol(hx, hz, level_);
            
            ++cur_vertex;
        }

        vb_.subData(dest,
                    width*sizeof(GridVertex),
                    &tmp_buffer_[0]);
        
        dest += resolution_*sizeof(GridVertex);
    }
}


//------------------------------------------------------------------------------
void ClipmapGrid::fillColormap(BUFFERFILL_CALLBACK_TYPE type,
                               unsigned dest_x, unsigned dest_y,
                               int source_x, int source_y,
                               unsigned width, unsigned height,
                               const ToroidalBuffer * buffer)
{
    if (type != BCT_FILL) return;

    unsigned f = terrain_data_->getLmTexelsPerQuad();
    
    RgbTriplet * dest = (RgbTriplet*)&tmp_buffer_[0];    
    for (unsigned y=0; y<height*f; ++y)
    {
        for (unsigned x=0; x<width*f; ++x)
        {
            *dest++ = terrain_data_->getColorAtGrid(source_x*f+x,
                                                   source_y*f+y,
                                                   level_);
            
        }
    }

    color_map_->apply(s_scene_manager.getOsgState());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    dest_x*f, dest_y*f,
                    width*f, height*f,
                    GL_RGB, GL_UNSIGNED_BYTE, &tmp_buffer_[0]);
}


//------------------------------------------------------------------------------
/**
 *
 */
void ClipmapGrid::fillIndexBuffer(BUFFERFILL_CALLBACK_TYPE type,
                                  unsigned dest_x, unsigned dest_y,
                                  int source_x, int source_y,
                                  unsigned width, unsigned height,
                                  const ToroidalBuffer * buffer)
{
    if (type != BCT_FILL) return;
    
    ToroidalIterator it1 = vb_torus_.getIterator(source_x, source_y);
    ToroidalIterator it2 = it1;
    it2.incY();

    unsigned dest = (quad_offset_[buffer - ib_torus_] +
                     dest_x + buffer->getWidth()*dest_y)*6*sizeof(uint16_t);
    
    for (unsigned y=0; y<height; ++y)
    {
        uint16_t * cur_index = (uint16_t*)&tmp_buffer_[0];
        assert(tmp_buffer_.size() >= width*6*sizeof(uint16_t));

        ToroidalIterator cur_it1 = it1;
        ToroidalIterator cur_it2 = it2;

        it1 = it2;
        it2.incY();
        
        for (unsigned x=0; x<width; ++x)
        {
            *cur_index++ = *cur_it1;
            *cur_index++ = *cur_it2;
            cur_it2.incX();
            *cur_index++ = *cur_it2;

            *cur_index++ = *cur_it1;
            cur_it1.incX();
            *cur_index++ = *cur_it2;
            *cur_index++ = *cur_it1;
        }
        
        ib_.subData(dest,
                    width*6*sizeof(uint16_t),
                    &tmp_buffer_[0]);
        
        dest += buffer->getWidth()*6*sizeof(uint16_t);
    }
}



//------------------------------------------------------------------------------
/**
 *  Responsible for zero-area triangles at the level border to avoid
 *  t-junctions and for L-shaped triangle strips of width 1 around
 *  child level.
 */
void ClipmapGrid::updateStitchingIndices()
{
    int x, y;
    vb_torus_.getPos(x,y);

    ToroidalIterator it1, it2, it3, it4;
    
    if (child_)
    {
        uint16_t * p_ind = &index_stitching_tris_[(resolution_>>1)*12];

        // If number of quads between left border and child left
        // border is even, the filling strip is needed right, else
        // left.
        bool strip_left = !(((child_->clip_region_x_ - clip_region_x_)>>level_) & 1);
        bool strip_top  = !(((child_->clip_region_z_ - clip_region_z_)>>level_) & 1);

        // Offset to top / left strip
        unsigned offset1 = (resolution_-3) >>2;
        // Offset to bottom / right strip
        unsigned offset2 = ((3*resolution_-1) >>2) -1;
        
        // Horizontal iterators
        it1 = vb_torus_.getIterator(x+offset1, y+ (strip_top ? offset1 : offset2));
        it2 = it1;
        it2.incY();

        // Vertical iterators
        it3 = vb_torus_.getIterator(x+(strip_left ? offset1 : offset2), y+ offset1 + strip_top);
        it4 = it3;
        it4.incX();
        
        for (int t=0; t<(resolution_>>1)+1; ++t)
        {
            *p_ind++ = *it1;
            *p_ind++ = *it2;
            it2.incX();
            *p_ind++ = *it2;

            *p_ind++ = *it1;
            it1.incX();
            *p_ind++ = *it2;
            *p_ind++ = *it1;
            
            if (t != resolution_>>1)
            {
                *p_ind++ = *it4;
                *p_ind++ = *it3;
                it4.incY();
                *p_ind++ = *it4;

                *p_ind++ = *it4;
                *p_ind++ = *it3;
                it3.incY();
                *p_ind++ = *it3;
            }
        }
    }


    
    // Zero-area triangles at the borders    
    it1 = vb_torus_.getIterator(x,               y);
    it2 = vb_torus_.getIterator(x+resolution_-1, y);
    it3 = vb_torus_.getIterator(x+resolution_-1, y+resolution_-1);
    it4 = vb_torus_.getIterator(x,               y+resolution_-1);

    uint16_t * p_ind = &index_stitching_tris_[0];
    for (int x=0; x<(resolution_>>1); ++x)
    {
        *p_ind++ = *it1;
        it1.incX();
        *p_ind++ = *it1;
        it1.incX();
        *p_ind++ = *it1;
        
        *p_ind++ = *it2;
        it2.incY();
        *p_ind++ = *it2;
        it2.incY();
        *p_ind++ = *it2;

        *p_ind++ = *it3;
        it3.decX();
        *p_ind++ = *it3;
        it3.decX();
        *p_ind++ = *it3;

        *p_ind++ = *it4;
        it4.decY();
        *p_ind++ = *it4;
        it4.decY();
        *p_ind++ = *it4;
    }
}


//------------------------------------------------------------------------------
void ClipmapGrid::updateOffsetUniforms()
{
    // offset to calculate texture coordinate from vertex position
    unsigned ox, oz;
    colormap_torus_.getOrigin(ox, oz);
    offset_uniform_->set(osg::Vec2(((int)(ox<<level_)-clip_region_x_) * terrain_data_->getHorzScale(),
                                   ((int)(oz<<level_)-clip_region_z_) * terrain_data_->getHorzScale()));
  
    if (parent_)
    {
        parent_->colormap_torus_.getOrigin(ox, oz);
        offset_parent_uniform_->set(
            osg::Vec2(((int)(ox<<(level_+1))-parent_->clip_region_x_) * terrain_data_->getHorzScale(),
                      ((int)(oz<<(level_+1))-parent_->clip_region_z_) * terrain_data_->getHorzScale()));
    }
}


//------------------------------------------------------------------------------
/**
 *  Check whether a clipmap part is visible. The reasoning is as follows:
 *
 *  -) The central, square grid is always visible.
 *  -) Other grids are split into four parts
 *  -) The positions of the vertices nearest to the camera are calculated from the gridpoints parameter.
 *  -) If those two vertices are invisible, the entire part is invisible.
 *  -) The camera never is high enough to see parts "behind" the camera.
 *
 *  \param gridpoints x and z grid index offset to clip_region_x_,
 *  clip_region_z_ of the points to test.
 *
 *  \return Whether the corresponding clipmap part is visible.
 */
bool ClipmapGrid::clipRegion(const unsigned * gridpoints) const
{
    Vector2d p1 = Vector2d((clip_region_x_+((int)gridpoints[0]<<level_)) * terrain_data_->getHorzScale(),
                           (clip_region_z_+((int)gridpoints[1]<<level_)) * terrain_data_->getHorzScale());
    Vector2d p2 = Vector2d((clip_region_x_+((int)gridpoints[2]<<level_)) * terrain_data_->getHorzScale(),
                           (clip_region_z_+((int)gridpoints[3]<<level_)) * terrain_data_->getHorzScale());


    const Camera & camera = s_scene_manager.getCamera();

    Vector2d camera_dir = -camera.getTransform().getZ();
    camera_dir.safeNormalize();
    Vector2d camera_pos = camera.getTransform().getTranslation();

    Vector2d pd1 = p1 - camera_pos;
    Vector2d pd2 = p2 - camera_pos;
    pd1.normalize();
    pd2.normalize();

    float cos_alpha1 = vecDot(&pd1, &camera_dir);
    float cos_alpha2 = vecDot(&pd2, &camera_dir);

    if (cos_alpha1 < 0.0f && cos_alpha2 < 0.0f) return false;

    
    if (acosf(std::min(cos_alpha1, 1.0f)) > camera.getFov() &&
        acosf(std::min(cos_alpha2, 1.0f)) > camera.getFov())
    {
        Vector2d dir_tilt = camera_dir.tilt();
        if (sign(vecDot(&dir_tilt, &pd1)) ==
            sign(vecDot(&dir_tilt, &pd2))) return false;
    }

    return true;
}



//------------------------------------------------------------------------------
void ClipmapGrid::setupToroidalBuffers()
{
    int posx = clip_region_x_ >> level_;
    int posz = clip_region_z_ >> level_;
    unsigned lightmap_res = resolution_ +1;

    vb_.setData(sizeof(GridVertex) * resolution_ * resolution_, NULL, GL_DYNAMIC_DRAW_ARB);
    vb_torus_.setSize(resolution_, resolution_);
    vb_torus_.setPos(posx, posz);
    vb_torus_.setBufferFillCallback(BufferFillCallback(this, &ClipmapGrid::fillVertexBuffer));  
    
    
    colormap_torus_.setSize(lightmap_res, lightmap_res);
    colormap_torus_.setPos(posx, posz);
    colormap_torus_.setBufferFillCallback(BufferFillCallback(this, &ClipmapGrid::fillColormap));



    if (child_)
    {
        int num_quads = 2*(size_horzpart_x_*size_horzpart_z_ +
                           size_vertpart_x_*size_vertpart_z_);

        // Must match full square minus previous level
        assert(num_quads == (resolution_-1)*(resolution_-1) - ((resolution_+1)*(resolution_+1)>>2));

        ib_.setData(num_quads*6*sizeof(uint16_t),
                    NULL, GL_STREAM_DRAW_ARB);


        ib_torus_[0].setSize(size_horzpart_x_, size_horzpart_z_);
        ib_torus_[1].setSize(size_horzpart_x_, size_horzpart_z_);
        ib_torus_[2].setSize(size_vertpart_x_, size_vertpart_z_);
        ib_torus_[3].setSize(size_vertpart_x_, size_vertpart_z_);

        ib_torus_[0].setPos(posx,                                posz);                                // upper horizontal
        ib_torus_[1].setPos(posx,                                posz+resolution_-size_horzpart_z_-1); // lower horizontal
        ib_torus_[2].setPos(posx,                                posz+size_horzpart_z_);               // left vertical
        ib_torus_[3].setPos(posx+resolution_-size_vertpart_x_-1, posz+size_horzpart_z_);               // right vertical
        
        quad_offset_[0] = 0;
        quad_offset_[1] = size_horzpart_x_*size_horzpart_z_;
        quad_offset_[2] = size_horzpart_x_*size_horzpart_z_ + quad_offset_[1];
        quad_offset_[3] = size_vertpart_x_*size_vertpart_z_ + quad_offset_[2];
        quad_offset_[4] = size_vertpart_x_*size_vertpart_z_ + quad_offset_[3];
        
        for (unsigned i=0; i<4; ++i)
        {
            ib_torus_[i].setBufferFillCallback(BufferFillCallback(this, &ClipmapGrid::fillIndexBuffer));
        }

        // Invisible tris + L-shaped filling strip around next lower level
        index_stitching_tris_.resize((resolution_>>1)*12 + resolution_*6);
    } else
    {
        ib_.setData((resolution_-1)*(resolution_-1)*6*sizeof(uint16_t),
                    NULL, GL_STREAM_DRAW_ARB);


        ib_torus_[0].setSize(resolution_-1, resolution_-1);
        ib_torus_[0].setPos(posx, posz);
        ib_torus_[0].setBufferFillCallback(BufferFillCallback(this, &ClipmapGrid::fillIndexBuffer));
        
        memset(quad_offset_, 0, sizeof(quad_offset_));

        index_stitching_tris_.resize((resolution_>>1)*12);
    }   
    
}


} // namespace terrain
