
#ifndef RACING_HEIGHTDATA_CLIENT_INCLUDED
#define RACING_HEIGHTDATA_CLIENT_INCLUDED



#include "TerrainData.h"


namespace terrain
{

//------------------------------------------------------------------------------
struct RgbTriplet
{
    RgbTriplet() {}
    RgbTriplet(uint8_t r, uint8_t g, uint8_t b) :
        r_(r), g_(g), b_(b) {}
    uint8_t r_;
    uint8_t g_;
    uint8_t b_;
};


//------------------------------------------------------------------------------
class TerrainDataClient : public TerrainData
{
 public:
    TerrainDataClient();
    virtual ~TerrainDataClient();
    
    virtual void load(const std::string & name);

    Color getColor(float x, float z) const;
    RgbTriplet getColorAtGrid(int x, int z, unsigned level) const;
    uint32_t getDetailAtGrid (int x, int z, unsigned level) const;
    uint32_t getDetailAtGridInterpol(int x, int z, unsigned level) const;
    unsigned getPrevalentDetail(float x, float z) const;
    
    unsigned getLmTexelsPerQuad() const;
 protected:

    virtual void reset();
    
    void loadFromImages(const std::string & path);

    
    std::vector<RgbTriplet> color_data_;
    std::vector<uint32_t>   detail_data_;

    unsigned lm_texels_per_quad_;
};


 
} // namespace terrain 

#endif // #ifndef RACING_HEIGHTDATA_INCLUDED
