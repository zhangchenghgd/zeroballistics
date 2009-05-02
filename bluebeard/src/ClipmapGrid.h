
#ifndef RACING_CLIPMAPGRID_INCLUDED
#define RACING_CLIPMAPGRID_INCLUDED


#include <osg/Drawable>
#include <osg/Texture2D>


#include "Vector.h"
#include "ToroidalBuffer.h"
#include "BufferObject.h"

namespace terrain
{

class TerrainDataClient;


//------------------------------------------------------------------------------
/**
 *  
 */
class ClipmapGrid : public osg::Drawable
{
 public:
    ClipmapGrid(unsigned level, ClipmapGrid * child);
    virtual ~ClipmapGrid();

    virtual Object* cloneType() const               { assert(false); return NULL; }
    virtual Object* clone(const osg::CopyOp&) const { assert(false); return NULL; }
    virtual const char* className() const           { return "ClipmapGrid"; }

    virtual osg::BoundingBox computeBound() const;
    virtual void drawImplementation(osg::RenderInfo& renderInfo) const;

    virtual bool supports (const osg::PrimitiveFunctor &) const;
    virtual void accept (osg::PrimitiveFunctor &) const;

    
    void setParent(ClipmapGrid * parent);

    void update();

    void getClipRegion(int & x, int & z) const;

    static void setTerrainData(const TerrainDataClient * terrain_data);
    static const TerrainDataClient * getTerrainData();
    static int getClipmapResolution();    

 protected:
    
    void fillVertexBuffer(BUFFERFILL_CALLBACK_TYPE type,
                          unsigned dest_x, unsigned dest_y,
                          int source_x, int source_y,
                          unsigned width, unsigned height,
                          const ToroidalBuffer * buffer);
    

    void fillColormap(BUFFERFILL_CALLBACK_TYPE type,
                      unsigned dest_x, unsigned dest_y,
                      int source_x, int source_y,
                      unsigned width, unsigned height,
                      const ToroidalBuffer * buffer);

    void fillIndexBuffer(BUFFERFILL_CALLBACK_TYPE type,
                         unsigned dest_x, unsigned dest_y,
                         int source_x, int source_y,
                         unsigned width, unsigned height,
                         const ToroidalBuffer * buffer);
    
    void updateStitchingIndices();

    void updateOffsetUniforms();

    bool clipRegion(const unsigned * gridpoints) const;

    void setupToroidalBuffers();

    
    /// \brief X-Index in height_data of the top-left corner vertex of
    /// the grid region. The size is stored in resolution_.
    int clip_region_x_; 
    /// \brief Z-Index in height_data of the top-left corner vertex of
    /// the clip region. The size is stored in resolution_.
    int clip_region_z_;
    
    bool vertices_changed_; ///< Used to signal the next level that
                            ///this level has shifted and the
                            ///stitching tris have to be updated.

    
    BufferObject vb_;         ///< The grid's vertex buffer of size resolution_^2.
    BufferObject ib_;
    unsigned quad_offset_[5];


    ToroidalBuffer vb_torus_; ///< Used for toroidal access on the
                              ///mapped vertex data.
    ToroidalBuffer colormap_torus_;
    ToroidalBuffer ib_torus_[4];

    std::vector<uint16_t> index_stitching_tris_;

    ClipmapGrid * child_;    ///< The enclosed, smaller grid.
    ClipmapGrid * parent_;   ///< The enclosing, larger grid.
    unsigned level_;         ///< Innermost clipmap level is 0. Vertex spacing is 2^level.

    
    osg::ref_ptr<osg::Uniform> offset_uniform_;
    osg::ref_ptr<osg::Uniform> offset_parent_uniform_;
    osg::ref_ptr<osg::Texture2D> color_map_;

    static int resolution_;
/**
 * \brief Top & bottom parts, across whole width
 * <pre>
 *    ______
 *    |_____| 
 *    | | | | 
 *    |_|_|_| 
 *    |_____|
 * </pre>
 */
    static unsigned size_horzpart_x_; 
    static unsigned size_horzpart_z_; ///< Top & bottom parts, across whole width
    static unsigned size_vertpart_x_; ///< Left & right parts
    static unsigned size_vertpart_z_; ///< Left & right parts

    static unsigned clip_gridpoint_[4][4]; ///< Offset from clip_region_x_,
                                    ///clip_region_z_ to the grid
                                    ///points used for view
                                    ///frustum clipping.

    
    static std::vector<unsigned char> tmp_buffer_; ///< Used for IB,
                                                   ///VB and tex
                                                   ///subdata updates

    static const TerrainDataClient * terrain_data_;
};

} // namespace terrain

#endif // #ifndef RACING_CLIPMAPGRID_INCLUDED
