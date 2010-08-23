
#include "TerrainDataClient.h"

#include <limits>


#include <osg/Image>
#include <osgDB/ReadFile>


#include "ParameterManager.h"
#include "Paths.h"

#undef min
#undef max

namespace terrain
{

//------------------------------------------------------------------------------
TerrainDataClient::TerrainDataClient() :
    lm_texels_per_quad_(0)
{
}


//------------------------------------------------------------------------------
TerrainDataClient::~TerrainDataClient()
{
}
    

//------------------------------------------------------------------------------
void TerrainDataClient::load(const std::string & name)
{
    TerrainData::load(name);

    loadFromImages(LEVEL_PATH + name + "/");
}



//------------------------------------------------------------------------------
Color TerrainDataClient::getColor(float x, float z) const
{
    x *= lm_texels_per_quad_ / horz_scale_;
    z *= lm_texels_per_quad_ / horz_scale_;
    RgbTriplet c = getColorAtGrid((int)x, (int)z, 0);

    return Color((float)c.r_ / 255.0f,
                 (float)c.g_ / 255.0f,
                 (float)c.b_ / 255.0f);
}


//------------------------------------------------------------------------------
RgbTriplet TerrainDataClient::getColorAtGrid(int x, int z, unsigned level) const
{
    x <<= level;
    z <<= level;

    x = clamp(x, 1, (int)(width_ *lm_texels_per_quad_ - 2));
    z = clamp(z, 1, (int)(height_*lm_texels_per_quad_ - 2));

    return color_data_[x + z*width_*lm_texels_per_quad_];
}


//------------------------------------------------------------------------------
uint32_t TerrainDataClient::getDetailAtGrid(int x, int z, unsigned level) const
{
    x <<= level;
    z <<= level;

    x = clamp(x, 1, (int)width_  - 2);
    z = clamp(z, 1, (int)height_ - 2);
    
    
    return detail_data_[x + z*width_];
}


//------------------------------------------------------------------------------
uint32_t TerrainDataClient::getDetailAtGridInterpol(int x, int z, unsigned level) const
{
    if (x&1 || z&1)
    {
        uint32_t d1 = getDetailAtGrid( x>>1,         z>>1,        level+1);
        uint32_t d2 = getDetailAtGrid((x>>1)+(x&1), (z>>1)+(z&1), level+1);

        uint32_t ret = 0;

        unsigned sum=0;
        unsigned max_component_val = 0;
        unsigned max_component_num = 0;
        for (unsigned d=0; d<8; ++d)
        {
            uint32_t mask = 0xf << (4*d);
            uint32_t val = (((d1&mask) >> 1) + ((d2&mask) >> 1)) & mask;
            val >>= (4*d);

            sum += val;
            if (val > max_component_val)
            {
                max_component_val = val;
                max_component_num = d;
            }
            
            
            ret |= val << (4*d);
        }

        if (sum != 15)
        {
            int diff = 15 - sum;
            assert(diff>0);

            uint32_t mask = 0xf << (4*max_component_num);
            uint32_t max_val = (ret & mask) >> (4*max_component_num);
            max_val += diff;

            ret &= ~mask;
            ret |= max_val << (4*max_component_num);
        }
        
        return ret;
        
    } else
    {
        return getDetailAtGrid(x,z,level);
    }    
}


//------------------------------------------------------------------------------
/**
 *  \see cExporter::exportTerrain for shifting details
 */
unsigned TerrainDataClient::getPrevalentDetail(float x, float z) const
{
    x /= horz_scale_;
    z /= horz_scale_;

    unsigned x_int = std::min((unsigned)x, width_-1);
    unsigned z_int = std::min((unsigned)z, height_-1);

    uint32_t detail = detail_data_[x_int + z_int*width_];
    unsigned ret = (unsigned)-1;
    unsigned ret_val = 0;
    for (unsigned i=0; i<8; ++i)
    {
        unsigned cur_shift = 2*4*(i%4) + 4*(1-i/4);
        unsigned cur_detail = (detail >> cur_shift) & 0xf;
        
        if (cur_detail > ret_val)
        {
            ret_val = cur_detail;
            ret = i;
        }
    }

    return ret;
}


//------------------------------------------------------------------------------
unsigned TerrainDataClient::getLmTexelsPerQuad() const
{
    return lm_texels_per_quad_;
}

//------------------------------------------------------------------------------
void TerrainDataClient::reset()
{
    std::vector<RgbTriplet> cd; color_data_. swap(cd);
    std::vector<uint32_t>   dd; detail_data_.swap(dd);    
}


//------------------------------------------------------------------------------
void TerrainDataClient::loadFromImages(const std::string & path)
{
    {
        osg::ref_ptr<osg::Image> color = osgDB::readImageFile(path + "lm_color.png");

        if (!color.get())             throw Exception("Cannot read lightmap.");
        if (color->s() % width_ != 0) throw Exception("LM size must be multiple of HM size.");
        
        color->flipVertical();

        lm_texels_per_quad_ = color->s() / width_;
        assert(color->t() / height_ == lm_texels_per_quad_);

        unsigned lm_width  = lm_texels_per_quad_*width_;
        unsigned lm_height = lm_texels_per_quad_*height_;
        
        color_data_.resize(lm_width*lm_height);

        unsigned bytes_per_pixel = color->getPixelSizeInBits() >> 3;

        const unsigned char * cur_line = color->data();
        for (unsigned r=0; r<lm_height; ++r)
        {
            const unsigned char * cur_src = cur_line;
            for (unsigned c=0; c<lm_width; ++c)
            {
                color_data_[c + r*lm_width].r_ = cur_src[0];
                if (bytes_per_pixel >= 3)
                {
                    color_data_[c + r*lm_width].g_ = cur_src[1];
                    color_data_[c + r*lm_width].b_ = cur_src[2];
                } else
                {
                    color_data_[c + r*lm_width].g_ = cur_src[0];
                    color_data_[c + r*lm_width].b_ = cur_src[0];
                }

                cur_src += bytes_per_pixel;
            }

            cur_line += color->getRowSizeInBytes();
        }

        color->flipVertical();
        
    }

    
    {
        osg::ref_ptr<osg::Image> detail = osgDB::readImageFile(path + "detail.png");


        if (!detail.get())
        {
            Exception e("Unable to load detail file ");
            e << path << ".";
            throw e;
        }
        if (detail->s() != (int)width_ ||
            detail->t() != (int)height_)
        {
            throw Exception("detail map is not the same size as hm");
        }
        if (detail->getPixelSizeInBits() != 32)
        {
            throw Exception("Detail map must be RGBA");
        }
        
        detail_data_.resize(width_*height_);

        detail->flipVertical();        
        uint32_t * cur_dest = &detail_data_[0];
        const uint8_t * cur_line = (uint8_t*)detail->data();
        for (unsigned row=0; row<height_; ++row)
        {
            memcpy(cur_dest, cur_line, sizeof(uint32_t)*width_);
            cur_line += detail->getRowSizeInBytes();
            cur_dest += width_;
        }
        detail->flipVertical();
    }
}


} // namespace terrain
