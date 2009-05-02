
#ifndef RACING_HEIGHTDATA_INCLUDED
#define RACING_HEIGHTDATA_INCLUDED

#include "Vector.h"
#include "Datatypes.h"

namespace physics
{
    struct CollisionInfo;
}


namespace terrain
{
 
//------------------------------------------------------------------------------
class TerrainData
{
 public:
    TerrainData();
    virtual ~TerrainData();

    virtual void load(const std::string & name);
    const std::string & getName() const;

    unsigned getResX() const;
    unsigned getResZ() const;
    
    float getHeightAtGrid(int x, int z, unsigned level) const;
    float getHeightAtGridInterpol(int x, int z,
                                  unsigned level) const;

    float getMinHeight() const;
    float getMaxHeight() const;

    float getHorzScale() const;

    const float * getHeightData() const;


    void getHeightAndNormal(float x, float z,
                            float & h, Vector & n) const;
    void getHeightAndNormalBicubic(float x, float z,
                                   float & h, Vector & n) const;

    bool collideRay(physics::CollisionInfo & info,
                    const Vector & tip,
                    const Vector & dir,
                    float penetration_guess,
                    bool bicubic) const;
protected:

    virtual void reset();

    void loadHm(const std::string & name);


    float c0(float frac3, float frac2, float frac) const { return -0.5f*frac3 +      frac2 - 0.5f*frac      ; }
    float c1(float frac3, float frac2, float frac) const { return  1.5f*frac3 - 2.5f*frac2               + 1; }
    float c2(float frac3, float frac2, float frac) const { return -1.5f*frac3 + 2.0f*frac2 + 0.5f*frac      ; }
    float c3(float frac3, float frac2, float frac) const { return  0.5f*frac3 - 0.5f*frac2                  ; }

    float c0d(float frac2, float frac) const { return -1.5f*frac2 + 2.0f*frac - 0.5f ; }
    float c1d(float frac2, float frac) const { return  4.5f*frac2 - 5.0f*frac        ; }
    float c2d(float frac2, float frac) const { return -4.5f*frac2 + 4.0f*frac + 0.5f ; }
    float c3d(float frac2, float frac) const { return  1.5f*frac2 - 1.0f*frac        ; }

    float getLinearFalloff(float x, float z) const;
    
    std::vector<float32_t>  height_data_; ///< The actual height data
                                          ///in one large array. Column
                                          ///major to match grome
                                          ///height data.
    
    uint32_t width_;                     ///< The width of the height_data_ array.
    uint32_t height_;                    ///< The width of the height_data_ array.

    float32_t max_height_; ///< The maximum value stored in height_data_.
    float32_t min_height_; ///< The minimum value stored in height_data_.

    float32_t horz_scale_; ///< The horizontal distance between two height
                       ///samples. Needed to determine e.g. normal
                       ///vector.

    std::string name_;
};

} // namespace terrain 

#endif // #ifndef RACING_HEIGHTDATA_INCLUDED
