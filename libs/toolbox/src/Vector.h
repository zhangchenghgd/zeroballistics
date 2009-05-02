
#ifndef LIB_VECTOR_INCLUDED
#define LIB_VECTOR_INCLUDED

#include <ostream>
#include <iomanip>
#include <vector>

#include <cmath>

#include "Vector2d.h"
#include "Datatypes.h"

class Vector
{
 public:
    Vector();
    Vector(float x, float y, float z);

    Vector(const double * pd);
    Vector(const float * pf);

    float length() const;
    float lengthSqr() const;
    void normalize();
    void safeNormalize();

    Vector & operator=(const Vector & v);
    Vector operator-() const;
    
    Vector operator+(const Vector & v) const;
    Vector operator-(const Vector & v) const;
    Vector operator*(float f) const;
    Vector operator/(float f) const;

    Vector & operator+=(const Vector & v);
    Vector & operator-=(const Vector & v);
    Vector & operator*=(float f);
    Vector & operator/=(float f);

    void getYawPitch(float & yaw, float & pitch);

    operator Vector2d() const;
    
    float32_t x_;
    float32_t y_;
    float32_t z_;
};

//------------------------------------------------------------------------------
inline Vector operator*(float f, const Vector & v)
{
    Vector ret;
    
    ret.x_ = v.x_*f;
    ret.y_ = v.y_*f;
    ret.z_ = v.z_*f;
    
    return ret;
}


//------------------------------------------------------------------------------
inline float vecDot(const Vector * v1, const Vector * v2)
{
    return v1->x_*v2->x_ + v1->y_*v2->y_ + v1->z_*v2->z_;
}

//------------------------------------------------------------------------------
inline float vecCosAlpha(const Vector * v1, const Vector * v2)
{
    return vecDot(v1, v2) / (v1->length()*v2->length());
}

//------------------------------------------------------------------------------
inline Vector * vecCross(Vector * res, const Vector * v1, const Vector * v2)
{
    Vector tmp;

    tmp.x_ = v1->y_*v2->z_ - v1->z_*v2->y_;
    tmp.y_ = v1->z_*v2->x_ - v1->x_*v2->z_;
    tmp.z_ = v1->x_*v2->y_ - v1->y_*v2->x_;

    *res = tmp;

    return res;
}

std::ostream & operator<<(std::ostream & out, const Vector & v);
std::istream & operator>>(std::istream & in, Vector & v);

namespace serializer
{
    
void putInto(Serializer & s, const Vector & v);
    
void getFrom(Serializer & s, Vector & v);

}

#endif // #ifndef LIB_VECTOR_INCLUDED
