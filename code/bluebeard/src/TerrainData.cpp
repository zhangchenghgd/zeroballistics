
#include "TerrainData.h"

#include <limits>

#include "physics/OdeCollision.h"

#include "Utils.h"
#include "Serializer.h"
#include "Paths.h"
#include "Geometry.h"


#ifndef DEDICATED_SERVER
#include "ParameterManager.h"
#include <osg/Image>
#include <osgDB/ReadFile>
#endif


#undef min
#undef max

namespace terrain
{

const float BORDER_SLOPE = -0.1;

    
//------------------------------------------------------------------------------
TerrainData::TerrainData() :
    width_(0), height_(0),
    max_height_(0.0f), min_height_(0.0f),
    horz_scale_(0.0f)
{
}

//------------------------------------------------------------------------------
TerrainData::~TerrainData()
{
}

//------------------------------------------------------------------------------
void TerrainData::load(const std::string & name)
{
    if (name == name_) return;

    reset();    
    name_ = name;


    loadHm(LEVEL_PATH + name + "/");
}

//------------------------------------------------------------------------------
const std::string & TerrainData::getName() const
{
    return name_;
}

//------------------------------------------------------------------------------
unsigned TerrainData::getResX() const
{
    return width_;
}

//------------------------------------------------------------------------------
unsigned TerrainData::getResZ() const
{
    return height_;
}


//------------------------------------------------------------------------------
/**
 *  Returns the height at a specified position in the given
 *  interpolation level (0 being full detail).
 */
float TerrainData::getHeightAtGrid(int x, int z, unsigned level) const
{    
    x *= 1<<level;
    z *= 1<<level;

    float dh = getLinearFalloff(x, z);

    x = clamp(x, 0, (int)width_  - 1);
    z = clamp(z, 0, (int)height_ - 1);
  
    return height_data_[x + z*width_] + dh;
}

//------------------------------------------------------------------------------
/**
 *  Returns the height value from the next higher level by
 *  interpolating next level's samples.
 */
float TerrainData::getHeightAtGridInterpol(int x, int z,
                                          unsigned level) const
{
    if (x&1 || z&1)
    {
        return 0.5f *
            (getHeightAtGrid(x>>1, z>>1, level+1) +
             getHeightAtGrid((x>>1)+(x&1), (z>>1)+(z&1), level+1));
    } else
    {
        return getHeightAtGrid(x,z,level);
    }
}

//------------------------------------------------------------------------------
float TerrainData::getMinHeight() const
{
    return min_height_;
}

//------------------------------------------------------------------------------
float TerrainData::getMaxHeight() const
{
    return max_height_;
}

//------------------------------------------------------------------------------
float TerrainData::getHorzScale() const
{
    return horz_scale_;
}

//------------------------------------------------------------------------------
const float * TerrainData::getHeightData() const
{
    return &height_data_[0];
}


//------------------------------------------------------------------------------
void TerrainData::getHeightAndNormal(float x, float z,
                                     float & h, Vector & n) const
{
    x /= horz_scale_;
    z /= horz_scale_;

    float dh = getLinearFalloff(x, z);
    
    if (x < 1.0f) x = 1.0f;
    else if (x > width_  - EPSILON - 2.0f) x =  width_  - EPSILON - 2.0f;
    if (z < 1.0f) z = 1.0f;
    else if (z > width_  - EPSILON - 2.0f) z =  width_  - EPSILON - 2.0f;
    
    unsigned x_int = (unsigned)x;
    unsigned z_int = (unsigned)z;
    float x_frac = x - x_int;
    float z_frac = z - z_int;

    const float * h_base = &height_data_[x_int + z_int*width_];

    float ab   = h_base[1]      - h_base[0];
    float ac   = h_base[width_] - h_base[0];
    float adbc = h_base[0]      + h_base[1+width_] - h_base[1] - h_base[width_];
    
    h = h_base[0] + x_frac*ab + z_frac*ac + x_frac*z_frac*adbc + dh;

    float hx = ab + z_frac*adbc;
    float hz = ac + x_frac*adbc;
    n = Vector(-hx,horz_scale_,-hz);
    n.normalize();
}



//------------------------------------------------------------------------------
void TerrainData::getHeightAndNormalBicubic(float x, float z,
                                            float & h, Vector & n) const
{
    x /= horz_scale_;
    z /= horz_scale_;

    float dh = getLinearFalloff(x, z);
    
    bool clamp_x = false, clamp_z = false;
    if (x < 1.0f)
    {
        clamp_x = true;
        x = 1.0f;
    } else if (x > width_  - EPSILON - 2.0f)
    {
        clamp_x = true;
        x =  width_  - EPSILON - 2.0f;
    }
    if (z < 1.0f)
    {
        clamp_z = true;
        z = 1.0f;
    } else if (z > width_  - EPSILON - 2.0f)
    {
        clamp_z = true;
        z =  width_  - EPSILON - 2.0f;
    }

    unsigned x_int = (unsigned)x;
    unsigned z_int = (unsigned)z;
    float x_frac = x - x_int;
    float z_frac = z - z_int;

    const float * h_base = &height_data_[x_int + z_int*width_];

    float x_frac2 = x_frac*x_frac;
    float x_frac3 = x_frac*x_frac2;
    float z_frac2 = z_frac*z_frac;
    float z_frac3 = z_frac*z_frac2;
    
    float cx[4] = { c0(x_frac3, x_frac2, x_frac),
                    c1(x_frac3, x_frac2, x_frac),
                    c2(x_frac3, x_frac2, x_frac),
                    c3(x_frac3, x_frac2, x_frac) };
    float cxd[4] = { c0d(x_frac2, x_frac),
                     c1d(x_frac2, x_frac),
                     c2d(x_frac2, x_frac),
                     c3d(x_frac2, x_frac) };
    float cz[4] = { c0(z_frac3, z_frac2, z_frac),
                    c1(z_frac3, z_frac2, z_frac),
                    c2(z_frac3, z_frac2, z_frac),
                    c3(z_frac3, z_frac2, z_frac) };
    float czd[4] = { c0d(z_frac2, z_frac),
                     c1d(z_frac2, z_frac),
                     c2d(z_frac2, z_frac),
                     c3d(z_frac2, z_frac) };
    
    float x_interpol [4] = { 0,0,0,0 };
    float xd_interpol[4] = { 0,0,0,0 };
    for (int i=0; i<4; ++i)
    {
        for (int j=0; j<4; ++j)
        {
            x_interpol [i] += h_base[j-1 + (i-1)*width_]*cx[j];
            xd_interpol[i] += h_base[j-1 + (i-1)*width_]*cxd[j];
        }
    }

    h = (x_interpol[0]*cz[0] +
         x_interpol[1]*cz[1] +
         x_interpol[2]*cz[2] +
         x_interpol[3]*cz[3]) + dh;
    float hz = (x_interpol[0]*czd[0] +
                x_interpol[1]*czd[1] +
                x_interpol[2]*czd[2] +
                x_interpol[3]*czd[3]);
    float hx = (xd_interpol[0]*cz[0] +
                xd_interpol[1]*cz[1] +
                xd_interpol[2]*cz[2] +
                xd_interpol[3]*cz[3]);

    n = Vector(-hx, horz_scale_, -hz);

    if (clamp_x) n.x_ = 0.0f;
    if (clamp_z) n.z_ = 0.0f;
    
    n.normalize();
}

//------------------------------------------------------------------------------
/**
 *  Collide a ray against the interpolated surface. Simple euler-style
 *  calculation. Construct a tangent plane at the best guess for the
 *  contact and intersect ray with that plane.
 *
 *  \param info [in,out] Contact data will be stored here, but only if
 *  the ray's penetration is greater than the existing penetration in
 *  the structure.
 *
 *  \param tip The "tip" of the ray.
 *
 *  \param dir The direction of the ray, or "up".
 *
 *  \param penetration_guess Used to calc best guess for intersection.
 *
 *  \param bicubic If true, do bicubic interpolation, bilinear otherwise.
 *
 *  \return Whether the collision info was updated.
 */
bool TerrainData::collideRay(physics::CollisionInfo & info,
                             const Vector & tip,
                             const Vector & dir,
                             float penetration_guess,
                             bool bicubic) const
{
    // Because we cannot do real line intersection with the
    // interpolated surface, use the penetration from last frame as
    // best guess for the current one.
    Vector guess = tip + penetration_guess*dir;

    float h;
    Vector n;
    if (bicubic) getHeightAndNormalBicubic(guess.x_, guess.z_, h, n);
    else         getHeightAndNormal       (guess.x_, guess.z_, h, n);        

    // pos lies on the interpolated surface
    Vector pos(guess.x_, h, guess.z_);


    // Now find an estimate for the intersection point euler-style by
    // constructing the tangent plane and intersecting it with our
    // ray.
    Plane p(pos, n);

    float d = -p.evalPoint(guess);
    float dot = vecDot(&n, &dir);
    if (dot < 0.0f) return false;
 
    // pen is the distance of guess from the plane p in direction
    // up. It also is the difference between this frame's penetration
    // and the previous frame's penetration.
    float pen = d / dot;

    if (pen + penetration_guess > info.penetration_)
    {
        info.penetration_ = pen + penetration_guess;
        info.n_           = dir;
        info.pos_         = guess + dir * pen;

        // pos must lie on plane
        // this happens because of numeric inaccuracies -> disable
        //assert(equalsZero(p.evalPoint(info.pos_)));

        return true;
    } else return false;
}


//------------------------------------------------------------------------------
void TerrainData::reset()
{
    name_ = "";

    std::vector<float32_t>  hd; height_data_.swap(hd);

    width_      = 0;
    height_     = 0;
    max_height_ = 0.0f;
    min_height_ = 0.0f;
    horz_scale_ = 0.0f;
}

//------------------------------------------------------------------------------
void TerrainData::loadHm(const std::string & path)
{
    if (existsFile((path + "terrain.hm").c_str()))
    {    
        serializer::Serializer s(path + "terrain.hm", serializer::SOM_READ);

        s.get(width_);
        s.get(height_);
        s.get(max_height_);
        s.get(min_height_);
        s.get(horz_scale_);

        height_data_.resize(width_*height_);

        s.getRaw(&height_data_[0], height_data_.size() * sizeof(float32_t));
    } else
    {
        Exception e("Couldn't load hm file from ");
        e << path;
        throw e;
    }
}

//------------------------------------------------------------------------------
float TerrainData::getLinearFalloff(float x, float z) const
{
    float dh = 0.0f;
    
    if      (x <  0)            dh += BORDER_SLOPE* -x;
    else if (x >= (int)width_)  dh += BORDER_SLOPE*(x-width_+1);
    if      (z <  0)            dh += BORDER_SLOPE* -z;
    else if (z >= (int)height_) dh += BORDER_SLOPE*(z-height_+1);

    return dh;
}

} // namespace terrain
