
#include "Vector2d.h"

#include "Matrix.h"
#include "utility_Math.h"
#include "Serializer.h"
#include "TextValue.h"
#include "Vector.h"
#include "Log.h"

//------------------------------------------------------------------------------
Vector2d::Vector2d()
{
}

//------------------------------------------------------------------------------
Vector2d::Vector2d(float x, float y) : x_(x), y_(y)
{
}

//------------------------------------------------------------------------------
Vector2d::Vector2d(float x, float y, float /* z */) : x_(x), y_(y)
{
}


//------------------------------------------------------------------------------
float Vector2d::length() const
{
    return sqrtf(x_*x_ + y_*y_);
}


//------------------------------------------------------------------------------
float Vector2d::lengthSqr() const
{
    return x_*x_ + y_*y_;
}

//------------------------------------------------------------------------------
void Vector2d::normalize()
{
    float inv_len = 1.0f/length();
    *this *= inv_len;
}

//------------------------------------------------------------------------------
void Vector2d::safeNormalize()
{
    float l = length();

    if (equalsZero(l))
    {
        x_ = 1.0f;
        s_log << Log::debug('N') << "vector has zero length in Vector2d::safeNormalize\n";
        return;
    }
    
    float inv_len = 1.0f/l;
    *this *= inv_len;
}


//------------------------------------------------------------------------------
Vector2d Vector2d::operator-() const
{
    return Vector2d(-x_, -y_);
}

//------------------------------------------------------------------------------
Vector2d Vector2d::operator+(const Vector2d & v2) const
{
    return Vector2d(x_ + v2.x_, y_ + v2.y_);
}


//------------------------------------------------------------------------------
Vector2d Vector2d::operator-(const Vector2d & v2) const
{
    return Vector2d(x_ - v2.x_, y_ - v2.y_);
}

//------------------------------------------------------------------------------
Vector2d Vector2d::operator*(float f) const
{
    return(Vector2d(x_ * f, y_ * f));
}

//------------------------------------------------------------------------------
Vector2d Vector2d::operator/(float f) const
{
    return(Vector2d(x_ / f, y_ / f));
}

//------------------------------------------------------------------------------
Vector2d & Vector2d::operator+=(const Vector2d & v)
{
    x_ += v.x_;
    y_ += v.y_;

    return *this;
}


//------------------------------------------------------------------------------
Vector2d & Vector2d::operator-=(const Vector2d & v)
{
    x_ -= v.x_;
    y_ -= v.y_;

    return *this;
}


//------------------------------------------------------------------------------
Vector2d & Vector2d::operator*=(float f)
{
    x_ *= f;
    y_ *= f;

    return *this;
}


//------------------------------------------------------------------------------
Vector2d & Vector2d::operator/=(float f)
{
    x_ /= f;
    y_ /= f;

    return *this;
}


//------------------------------------------------------------------------------
float Vector2d::operator*(const Vector2d & v2) const
{
    return x_*v2.x_ + y_*v2.y_;
}



//------------------------------------------------------------------------------
/**
 *  Rotates the vector 90 degree.
 */
Vector2d Vector2d::tilt() const
{
    return Vector2d(y_, -x_);
}

//------------------------------------------------------------------------------
Vector2d::operator Vector() const
{
    static Vector v(0.0f, 0.0f, 0.0f);
    v.x_ = x_;
    v.z_ = y_;
    return v;
}


//------------------------------------------------------------------------------
Vector2d operator*(float f, Vector2d v)
{
    return v*f;
}


//------------------------------------------------------------------------------
float vecCross(const Vector2d * v1, const Vector2d * v2)
{
    return v1->y_*v2->x_ - v1->x_*v2->y_;
}


//------------------------------------------------------------------------------
float vecCosAlpha(const Vector2d * v1, const Vector2d * v2)
{
    return vecDot(v1, v2) / (v1->length()*v2->length());
}

//------------------------------------------------------------------------------
float vecDot(const Vector2d * v1, const Vector2d * v2)
{
    return v1->x_*v2->x_ + v1->y_*v2->y_;
}

//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const Vector2d & v)
{
    out << std::setw(6);
    out << "[" << v.x_
        << ";" << v.y_
        << "]";
    
    return out;
}

//------------------------------------------------------------------------------
std::istream & operator>>(std::istream & in, Vector2d & v)
{
    std::vector<float> values;
    in >> values;
    if (values.size() != 2) throw Exception("Invalid number of elements for vector2d.");

    v.x_ = values[0];
    v.y_ = values[1];

    return in;
}

namespace serializer
{
    
//------------------------------------------------------------------------------
void putInto(Serializer & s, const Vector2d & v)
{
    s.put(v.x_);
    s.put(v.y_);
}

//------------------------------------------------------------------------------
void getFrom(Serializer & s, Vector2d & v)
{
    s.get(v.x_);
    s.get(v.y_);
}

} // namespace serializer
