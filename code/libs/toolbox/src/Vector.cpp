
#include "Vector.h"

#include "utility_Math.h"
#include "Serializer.h"
#include "TextValue.h"
#include "Log.h"


//------------------------------------------------------------------------------
Vector::Vector()
{
}

//------------------------------------------------------------------------------
Vector::Vector(float x, float y, float z):x_(x), y_(y), z_(z)
{
}

//------------------------------------------------------------------------------
Vector::Vector(const double * pd) :
    x_(pd[0]),
    y_(pd[1]),
    z_(pd[2])
{
}

//------------------------------------------------------------------------------
Vector::Vector(const float * pf)
{
    memcpy(this, pf, sizeof(*this));
}


//------------------------------------------------------------------------------
float Vector::length() const
{
    return sqrtf(x_*x_ + y_*y_ + z_*z_);
}


//------------------------------------------------------------------------------
float Vector::lengthSqr() const
{
    return x_*x_ + y_*y_ + z_*z_;
}


//------------------------------------------------------------------------------
void Vector::normalize()
{
    float inv_len = 1.0f/length();
    *this *= inv_len;
}

//------------------------------------------------------------------------------
void Vector::safeNormalize()
{
    float l = length();

    if (equalsZero(l))
    {
        x_ = 1.0f;
        s_log << Log::debug('N') << "vector has zero length in safeNormalize\n";
        return;
    }
    
    float inv_len = 1.0f/l;
    *this *= inv_len;
}


//------------------------------------------------------------------------------
Vector & Vector::operator=(const Vector & v)
{
    x_ = v.x_;
    y_ = v.y_;
    z_ = v.z_;


    return *this;
}


//------------------------------------------------------------------------------
Vector Vector::operator-() const
{
    Vector ret;
    
    ret.x_ = -x_;
    ret.y_ = -y_;
    ret.z_ = -z_;
    
    return ret;
}

//------------------------------------------------------------------------------
Vector Vector::operator+(const Vector & v) const
{
    Vector ret;
    
    ret.x_ = x_+v.x_;
    ret.y_ = y_+v.y_;
    ret.z_ = z_+v.z_;
    
    return ret;
}


//------------------------------------------------------------------------------
Vector Vector::operator-(const Vector & v) const
{
    Vector ret;
    
    ret.x_ = x_-v.x_;
    ret.y_ = y_-v.y_;
    ret.z_ = z_-v.z_;
    
    return ret;
}


//------------------------------------------------------------------------------
Vector Vector::operator*(float f) const
{
    Vector ret;
    
    ret.x_ = x_*f;
    ret.y_ = y_*f;
    ret.z_ = z_*f;
    
    return ret;
}


//------------------------------------------------------------------------------
Vector Vector::operator/(float f) const
{
    Vector ret;

    float inv = 1.0f / f;
    
    ret.x_ = x_*inv;
    ret.y_ = y_*inv;
    ret.z_ = z_*inv;
    
    return ret;
}


//------------------------------------------------------------------------------
Vector & Vector::operator+=(const Vector & v)
{
    x_ += v.x_;
    y_ += v.y_;
    z_ += v.z_;

    return *this;
}


//------------------------------------------------------------------------------
Vector & Vector::operator-=(const Vector & v)
{
    x_ -= v.x_;
    y_ -= v.y_;
    z_ -= v.z_;

    return *this;
}

//------------------------------------------------------------------------------
Vector & Vector::operator*=(float f)
{
    x_ *= f;
    y_ *= f;
    z_ *= f;

    return *this;
}

//------------------------------------------------------------------------------
Vector & Vector::operator/=(float f)
{
    float inv = 1.0f / f;

    x_ *= inv;
    y_ *= inv;
    z_ *= inv;

    return *this;
}

//------------------------------------------------------------------------------
void Vector::getYawPitch(float & yaw, float & pitch)
{
    if (equalsZero(z_))
    {
        yaw = x_ > 0.0f ? -PI*0.5f : PI*0.5f;
    } else
    {
        yaw = atanf(-x_ / z_);
        if (z_ > 0.0f) yaw += PI;
    }

    float base_length = sqrtf(sqr(x_)+sqr(z_));

    if (equalsZero(base_length))
    {
        pitch = y_ > 0.0f ? -PI*0.5f : PI*0.5f;
    } else pitch = atanf(-y_ / base_length);

}


//------------------------------------------------------------------------------
Vector::operator Vector2d() const
{
    return Vector2d(x_, z_);
}


//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const Vector & v)
{
    out << "[" << v.x_
        << ";" << v.y_
        << ";" << v.z_
        << "]";
    
    return out;
}

//------------------------------------------------------------------------------
std::istream & operator>>(std::istream & in, Vector & v)
{
    std::vector<float> values;
    in >> values;
    if (values.size() != 3) throw Exception("Invalid number of elements for vector.");

    v.x_ = values[0];
    v.y_ = values[1];
    v.z_ = values[2];

    return in;
}



namespace serializer
{

    
//------------------------------------------------------------------------------
void putInto(Serializer & s, const Vector & v)
{
    s.put(v.x_);
    s.put(v.y_);
    s.put(v.z_);
}

//------------------------------------------------------------------------------
void getFrom(Serializer & s, Vector & v)
{
    s.get(v.x_);
    s.get(v.y_);
    s.get(v.z_);
}
    
} // namespace serializer
