
#include "Plane.h"

#include "Matrix.h"



//------------------------------------------------------------------------------
Plane::Plane()
{
}


//------------------------------------------------------------------------------
Plane::Plane(float a, float b, float c, float d) :
    normal_(a,b,c), d_(d)
{
}


//------------------------------------------------------------------------------
/**
 *  Constructs a plane containing the three specified points.
 */
Plane::Plane(const Vector & p0, const Vector & p1, const Vector & p2)
{
    create(p0, p1, p2);
}

//------------------------------------------------------------------------------
Plane::Plane(const Vector & point, const Vector & normal)
{
    create(point, normal);
}


//------------------------------------------------------------------------------
void Plane::create(const Plane & p)
{
    *this = p;
}

//------------------------------------------------------------------------------
void Plane::create(const Vector & p0, const Vector & p1, const Vector & p2)
{
    Vector p0p1 = p1 - p0;
    Vector p0p2 = p2 - p0;
    vecCross(&normal_, &p0p1, & p0p2);

    normal_.normalize();

    setPointOnPlane(p0);
}


//------------------------------------------------------------------------------
/*
 *  Constructs a plane with a specified point lying on the plane and a specified
 *  normal.
 *  \todo needed somewhere time critical?
 */
void Plane::create(const Vector & point, const Vector & normal)
{
    normal_ = normal;
    normal_.normalize();

    d_ = - vecDot(&normal_, &point);
}


//------------------------------------------------------------------------------
/**
 *  Flips the orientation of the plane.
 */
void Plane::invert()
{
    d_      = -d_;
    normal_ = -normal_;
}

//------------------------------------------------------------------------------
/**
 *  Adjusts the parameter d of the plane equation so the specified
 *  point lies on the plane while the normal vector remains unchanged.
 */
void Plane::setPointOnPlane(const Vector & point)
{
    d_ = - vecDot(&normal_, &point);
}


//------------------------------------------------------------------------------
/**
 *  Evaluates the plane equation for the specified point.
 */
float Plane::evalPoint(const Vector & point) const
{
    return vecDot(&normal_, &point) + d_;
}


//------------------------------------------------------------------------------
void Plane::transform(const Matrix & mat)
{
    Vector p1 = mat.transformPoint(-d_*normal_);
    Vector p2 = mat.transformPoint((-d_ + 1.0f)*normal_);

    normal_ = p2-p1;
    normal_.normalize();
    setPointOnPlane(p1);
}
