

#include "Quaternion.h"


#include "utility_Math.h"

#undef min
#undef max

//------------------------------------------------------------------------------
Quaternion::Quaternion()
{
}

//------------------------------------------------------------------------------
Quaternion::Quaternion(float x, float y, float z, float w) :
    x_(x), y_(y), z_(z), w_(w)
{
}



//------------------------------------------------------------------------------
/**
 *  Preconditions: mat must be orthogonal
 *
 *  http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
 */
Quaternion::Quaternion(const Matrix & mat)
{
    w_ = sqrtf( std::max( 0.0f, 1.0f + mat._11 + mat._22 + mat._33 ) ) * 0.5f; 
    x_ = sqrtf( std::max( 0.0f, 1.0f + mat._11 - mat._22 - mat._33 ) ) * 0.5f; 
    y_ = sqrtf( std::max( 0.0f, 1.0f - mat._11 + mat._22 - mat._33 ) ) * 0.5f; 
    z_ = sqrtf( std::max( 0.0f, 1.0f - mat._11 - mat._22 + mat._33 ) ) * 0.5f; 

    x_ = copysign( x_, mat._32 - mat._23 );
    y_ = copysign( y_, mat._13 - mat._31 );
    z_ = copysign( z_, mat._21 - mat._12 );
}

//------------------------------------------------------------------------------
Quaternion Quaternion::operator-(const Quaternion & other) const
{
    return Quaternion(x_ - other.x_,
                      y_ - other.y_,
                      z_ - other.z_,
                      w_ - other.w_);
}


//------------------------------------------------------------------------------
float Quaternion::length() const
{
    return x_*x_ + y_*y_ + z_*z_ + w_*w_;
}

//------------------------------------------------------------------------------
float Quaternion::lengthSqr() const
{
    return sqrtf(length());
}

//------------------------------------------------------------------------------
void Quaternion::normalize()
{
    float f = 1.0f / length();

    x_ *= f;
    y_ *= f;
    z_ *= f;
    w_ *= f;
}


//------------------------------------------------------------------------------
/**
 *  Leaves translation unchanged.
 *
 *  http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
 */
void Quaternion::toMatrix(Matrix & mat) const
{
    float sqw = w_*w_;
    float sqx = x_*x_;
    float sqy = y_*y_;
    float sqz = z_*z_;
    mat._11 =  sqx - sqy - sqz + sqw; // since sqw + sqx + sqy + sqz =1
    mat._22 = -sqx + sqy - sqz + sqw;
    mat._33 = -sqx - sqy + sqz + sqw;
    
    float tmp1 = x_*y_;
    float tmp2 = z_*w_;
    mat._21 = 2.0f * (tmp1 + tmp2);
    mat._12 = 2.0f * (tmp1 - tmp2);
    
    tmp1 = x_*z_;
    tmp2 = y_*w_;
    mat._31 = 2.0f * (tmp1 - tmp2);
    mat._13 = 2.0f * (tmp1 + tmp2);
    tmp1 = y_*z_;
    tmp2 = x_*w_;
    mat._32 = 2.0f * (tmp1 + tmp2);
    mat._23 = 2.0f * (tmp1 - tmp2);

}
