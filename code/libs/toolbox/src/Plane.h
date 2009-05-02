
#ifndef LIB_PLANE_INCLUDED
#define LIB_PLANE_INCLUDED

#include <string>

#include "Vector.h"


//------------------------------------------------------------------------------
/**
 *  A plane satisfying the plane equation normal_*X + d_ = 0
 */
class Plane
{
 public:
    Plane();
    Plane(float a, float b, float c, float d);
    Plane(const Vector & p0, const Vector & p1, const Vector & p2);
    Plane(const Vector & point, const Vector & normal);

    void create(const Plane & p);
    void create(const Vector & p0, const Vector & p1, const Vector & p2);
    void create(const Vector & point, const Vector & normal);

    void invert();
    void setPointOnPlane(const Vector & point);
    float evalPoint(const Vector & point) const;

    void transform(const Matrix & mat);
    
    Vector normal_; ///< Normal of the plane (unit length)
    float d_;
};

#endif // #ifndef LIB_PLANE_INCLUDED
