
#include "Matrix.h"

#include "TextValue.h"
#include "utility_Math.h"
#include "Vector2d.h"
#include "Serializer.h"

const float MATRIX_EPSILON = 1.0e-8f;

//------------------------------------------------------------------------------
Matrix::Matrix()
{
}

//------------------------------------------------------------------------------
/**
 *  Constructs a matrix initialized to identity.
 */
Matrix::Matrix(bool b)
{
    assert(b);
    loadIdentity();
}

//------------------------------------------------------------------------------
/**
 *  Creates a matrix from 16 double elements.
 */
Matrix::Matrix(const double * pd)
{
    *this = pd;
}


//------------------------------------------------------------------------------
Matrix::Matrix(const float * pf)
{
    *this = pf;
}

//------------------------------------------------------------------------------
/**
 *  Creates a matrix from 16 double elements.
 */
Matrix & Matrix::operator=(const double * pd)
{
    for (unsigned i=0; i<16; ++i)
    {
        ((float*)this)[i] = pd[i];
    }

    return *this;
}


//------------------------------------------------------------------------------
Matrix & Matrix::operator=(const float * pf)
{
    memcpy(this, pf, sizeof(*this));

    return *this;
}


//------------------------------------------------------------------------------
/**
 *  Constructs a matrix initialized to specified values. Column 4
 *  specifies the translation component.
 */
Matrix::Matrix( float32_t r1c1, float32_t r1c2, float32_t r1c3, float32_t r1c4,
                float32_t r2c1, float32_t r2c2, float32_t r2c3, float32_t r2c4,
                float32_t r3c1, float32_t r3c2, float32_t r3c3, float32_t r3c4,
                float32_t r4c1, float32_t r4c2, float32_t r4c3, float32_t r4c4)
{
    _11 = r1c1;
    _21 = r2c1;
    _31 = r3c1;
    _41 = r4c1;
    
    _12 = r1c2;
    _22 = r2c2;
    _32 = r3c2;
    _42 = r4c2;

    _13 = r1c3;
    _23 = r2c3;
    _33 = r3c3;
    _43 = r4c3;

    _14 = r1c4; // Translation x
    _24 = r2c4; // Translation y
    _34 = r3c4; // Translation z
    _44 = r4c4;
}

//------------------------------------------------------------------------------
void Matrix::loadIdentity()
{
    memset(this, 0, sizeof(Matrix));
    _11 = _22 = _33 = _44 = 1.0f;
}

//------------------------------------------------------------------------------
/**
 *  Generates a rotation matrix around the x, y or z axis.
 *
 *  \param angle The rotation angle in radians.
 *  \param axis  '0' is x-axis, '1' is y, '2' is z.
 */
void Matrix::loadCanonicalRotation(float angle, unsigned char axis)
{
    float cos_alpha = cosf(angle);
    float sin_alpha = sinf(angle);

    loadIdentity();
    
    switch (axis)
    {
    case 0:
        _22 = cos_alpha;
        _23 = sin_alpha;
        
        _32 = -sin_alpha;
        _33 = cos_alpha;
        break;
    case 1:
        _11 = cos_alpha;
        _13 = sin_alpha;        
        
        _31 = -sin_alpha;
        _33 = cos_alpha;
        break;
    case 2:
        _11 = cos_alpha;
        _12 = sin_alpha;
        
        _21 = -sin_alpha;
        _22 = cos_alpha;
        break;
    }
}

//------------------------------------------------------------------------------
/**
 *  Creates a rotation matrix around an arbitrary axis.
 *
 *  \param angle The rotation angle in radians.
 *  \param v The rotation axis. Does not need to be normalized.
 */
void Matrix::loadRotationVector(float angle, const Vector & v)
{
    assert(!equalsZero(v.length()));
           
    Vector axis = v;
    axis.normalize();

    float sin_alpha = sinf(angle);
    float cos_alpha = cosf(angle);
    float one_minus_cos_alpha = 1.0f-cos_alpha;

    loadIdentity();

    _11 = axis.x_*axis.x_                     + cos_alpha*(1.0f - axis.x_*axis.x_);
    _21 = axis.x_*axis.y_*one_minus_cos_alpha - sin_alpha*axis.z_;
    _31 = axis.x_*axis.z_*one_minus_cos_alpha + sin_alpha*axis.y_;

    _12 = axis.x_*axis.y_*one_minus_cos_alpha + sin_alpha*axis.z_;
    _22 = axis.y_*axis.y_                     + cos_alpha*(1.0f - axis.y_*axis.y_);
    _32 = axis.y_*axis.z_*one_minus_cos_alpha - sin_alpha*axis.x_;
	
    _13 = axis.x_*axis.z_*one_minus_cos_alpha - sin_alpha*axis.y_;
    _23 = axis.y_*axis.z_*one_minus_cos_alpha + sin_alpha*axis.x_;
    _33 = axis.z_*axis.z_                     + cos_alpha*(1.0f - axis.z_*axis.z_);
}

//------------------------------------------------------------------------------
/*
 *  Calculates the orientation matrix with an up and direction vector.
 *
 *  First calculates the new x-axis (=right) as cross product between
 *  dir and up. Then the new y-axis (=up) is set to the cross product
 *  between the new x-axis and dir. Finally, the new z-axis is set to
 *  the negative direction.
 *
 *  dir and up don't have to be normalized. If dir and up are
 *  collinear, an up vector will be constructed.
 *
 *  Everything not in 3x3 of the matrix is left unchanged.
 */
void Matrix::loadOrientation(const Vector & dir, const Vector & up)
{
    Vector new_x;
    vecCross(&new_x, &dir, &up);

    if (equalsZero(new_x.lengthSqr(), MATRIX_EPSILON))
    {
        // dir and up point to same direction - invert coordinate of
        // dir and shift to find new up vector
        Vector new_up;

        new_up.x_ = -dir.y_;
        new_up.y_ = dir.z_;
        new_up.z_ = dir.x_;        
        
        vecCross(&new_x, &dir, &new_up);
    }

    getX() = new_x;
        
    vecCross(&getY(), &new_x, &dir);
    getZ() = -dir;

    getX().normalize();
    getY().normalize();
    getZ().normalize();
}



//------------------------------------------------------------------------------
/**
 *  Calculates the orientation matrix from the pitch and yaw angles.
 *  This is just the two rotation matrices multiplied.
 */
void Matrix::loadOrientationPart(float yaw, float pitch)
{
    float cos_pitch = cosf(pitch);
    float sin_pitch = sinf(pitch);
    float cos_yaw = cosf(yaw);
    float sin_yaw = sinf(yaw);
    
    _11 = cos_yaw;
    _21 = 0.0f;
    _31 = sin_yaw;

    _12 = sin_yaw*sin_pitch;
    _22 = cos_pitch;
    _32 = -cos_yaw*sin_pitch;
    
    _13 = -sin_yaw*cos_pitch;
    _23 = sin_pitch;
    _33 = cos_yaw*cos_pitch;
}


//------------------------------------------------------------------------------
/**
 *  Generates a scaling matrix.
 *
 *  \param x    The scaling factor in x-direction.
 *  \param y    The scaling factor in y-direction.
 *  \param z    The scaling factor in z-direction.
 */
void Matrix::loadScaling(float x, float y, float z)
{
    loadIdentity();

    _11 = x;
    _22 = y;
    _33 = z;
}

//------------------------------------------------------------------------------
/**
 *  Tailor-made for osg::Matrix::makeRotate.
 */
void Matrix::getEuler(float & x, float & y, float & z) const
{
    if (equalsZero(1.0f - _31, MATRIX_EPSILON))
    {
        x = atan2f(-_12, _22);
        y = -PI*0.5f;
        z = 0.0f;
    } else if (equalsZero(1.0f + _31, MATRIX_EPSILON))
    {
        x = atan2f(_12, _22);
        y = PI*0.5f;
        z = 0.0f;
    } else
    {    
        x = atan2f(_32, _33);
        y = asinf(-_31);
        z = atan2f(_21, _11);
    }
}


//------------------------------------------------------------------------------
/**
 *  Makes sure that mat times mat^T equals identity.
 *  This is done by making sure the new base vectors are orthonormal.
 *
 *  \todo perform only if neccessary
 */
void Matrix::orthonormalize()
{
/*     if (abs(vecDot((Vector*)&_11, (Vector*)&_12)) < 0.001f && */
/*         abs(vecDot((Vector*)&_11, (Vector*)&_13)) < 0.001f) return mat; */

    vecCross(&getZ(), &getX(), &getY());
    vecCross(&getY(), &getZ(), &getX());

    float l1 = getX().length();
    float l2 = getY().length();
    float l3 = getZ().length();

    assert(!equalsZero(l1));
    assert(!equalsZero(l2));
    assert(!equalsZero(l3));

    getX() /= l1;
    getY() /= l2;
    getZ() /= l3;
}


//------------------------------------------------------------------------------
void Matrix::invertAffine()
{
    *this = getAffineInverse();
}


//------------------------------------------------------------------------------
/**
 *  Inverts a 4x4 matrix composed only of translation & rotation.
 */
Matrix Matrix::getAffineInverse() const
{
    Matrix ret;
    
    ret._11 = _11;
    ret._21 = _12;
    ret._31 = _13;
    ret._41 = 0.0f;
    
    ret._12 = _21;
    ret._22 = _22;
    ret._32 = _23;
    ret._42 = 0.0f;

    ret._13 = _31;
    ret._23 = _32;
    ret._33 = _33;
    ret._43 = 0.0f;
    
    ret._14 = -(_14*_11 + _24*_21 + _34*_31);
    ret._24 = -(_14*_12 + _24*_22 + _34*_32);
    ret._34 = -(_14*_13 + _24*_23 + _34*_33);
    ret._44 = 1.0f;

    return ret;
}

//------------------------------------------------------------------------------
void Matrix::invert3x3()
{
    *this = get3x3Inverse();
}

//------------------------------------------------------------------------------
/**
 *  Unoptimized thus expensive 3x3 matrix inversion. No checking if matrix
 *  is invertible is done.
 */
Matrix Matrix::get3x3Inverse() const
{
    float cofac_11 =  (_22*_33 - _32*_23);
    float cofac_21 = -(_12*_33 - _32*_13);
    float cofac_31 =  (_12*_23 - _22*_13);
    
    float cofac_12 = -(_21*_33 - _31*_23);
    float cofac_22 =  (_11*_33 - _31*_13);
    float cofac_32 = -(_11*_23 - _21*_13);

    float cofac_13 =  (_21*_32 - _31*_22);
    float cofac_23 = -(_11*_32 - _31*_12);
    float cofac_33 =  (_11*_22 - _21*_12);

    float det = _11*cofac_11 + _12*cofac_12 + _13*cofac_13;
    float det_inv = 1.0f / det;

    Matrix ret;
    
    ret._11 = cofac_11 * det_inv;
    ret._21 = cofac_12 * det_inv;
    ret._31 = cofac_13 * det_inv;
    ret._41 = 0.0f;
    
    ret._12 = cofac_21 * det_inv;
    ret._22 = cofac_22 * det_inv;
    ret._32 = cofac_23 * det_inv;
    ret._42 = 0.0f;

    ret._13 = cofac_31 * det_inv;
    ret._23 = cofac_32 * det_inv;
    ret._33 = cofac_33 * det_inv;
    ret._43 = 0.0f;
    
    ret._14 = -(_14*ret._11 + _24*ret._12 + _34*ret._13);
    ret._24 = -(_14*ret._21 + _24*ret._22 + _34*ret._23);
    ret._34 = -(_14*ret._31 + _24*ret._32 + _34*ret._33);    
    ret._44 = 1.0f;
    
    return ret;
}

//------------------------------------------------------------------------------
Matrix Matrix::getTranspose3x3() const
{
    Matrix ret = *this;
    
    std::swap(ret._12, ret._21);
    std::swap(ret._13, ret._31);
    std::swap(ret._23, ret._32);

    return ret;
}

//------------------------------------------------------------------------------
Matrix Matrix::getTranspose4x4() const
{
    Matrix ret = *this;
    
    std::swap(ret._12, ret._21);
    std::swap(ret._13, ret._31);
    std::swap(ret._23, ret._32);

    std::swap(ret._14, ret._41);
    std::swap(ret._24, ret._42);
    std::swap(ret._34, ret._43);

    return ret;
}


//------------------------------------------------------------------------------
float Matrix::determinant() const
{
    float cofac_11 =  (_22*_33 - _32*_23);
    float cofac_12 = -(_21*_33 - _31*_23);
    float cofac_13 =  (_21*_32 - _31*_22);

    return _11*cofac_11 + _12*cofac_12 + _13*cofac_13;
}



//------------------------------------------------------------------------------
Vector & Matrix::getX() const
{
    return *(Vector*)&_11;
}

//------------------------------------------------------------------------------
Vector & Matrix::getY() const
{
    return *(Vector*)&_12;
}

//------------------------------------------------------------------------------
Vector & Matrix::getZ() const
{
    return *(Vector*)&_13;
}

//------------------------------------------------------------------------------
/**
 *  Retrieves the translation component of the matrix.
 */
Vector & Matrix::getTranslation() const
{
    return *(Vector*)&_14;
}


//------------------------------------------------------------------------------
Matrix & Matrix::operator+=(const Matrix & mat)
{
    _11 += mat._11;
    _21 += mat._21;
    _31 += mat._31;
    _41 += mat._41;

    _12 += mat._12;
    _22 += mat._22;
    _32 += mat._32;
    _42 += mat._42;

    _13 += mat._13;
    _23 += mat._23;
    _33 += mat._33;
    _43 += mat._43;

    _14 += mat._14;
    _24 += mat._24;
    _34 += mat._34;
    _44 += mat._44;

    return *this;
}

//------------------------------------------------------------------------------
Matrix & Matrix::operator*=(const Matrix & mat)
{
    *this = *this*mat;
    return *this;
}


//------------------------------------------------------------------------------
Matrix & Matrix::operator*=(float f)
{
    _11 *= f;
    _21 *= f;
    _31 *= f;
    _41 *= f;

    _12 *= f;
    _22 *= f;
    _32 *= f;
    _42 *= f;

    _13 *= f;
    _23 *= f;
    _33 *= f;
    _43 *= f;

    _14 *= f;
    _24 *= f;
    _34 *= f;
    _44 *= f;

    return *this;
}

//------------------------------------------------------------------------------
Matrix & Matrix::operator/=(float f)
{
    _11 /= f;
    _21 /= f;
    _31 /= f;
    _41 /= f;

    _12 /= f;
    _22 /= f;
    _32 /= f;
    _42 /= f;

    _13 /= f;
    _23 /= f;
    _33 /= f;
    _43 /= f;

    _14 /= f;
    _24 /= f;
    _34 /= f;
    _44 /= f;

    return *this;
}

//------------------------------------------------------------------------------
Matrix Matrix::operator-(const Matrix & mat) const
{
    return Matrix( _11-mat._11, _12-mat._12, _13-mat._13, _14-mat._14,
                   _21-mat._21, _22-mat._22, _23-mat._23, _24-mat._24,
                   _31-mat._31, _32-mat._32, _33-mat._33, _34-mat._34,
                   _41-mat._41, _42-mat._42, _43-mat._43, _44-mat._44);
}

//------------------------------------------------------------------------------
Matrix Matrix::operator*(const Matrix & mat) const
{
    Matrix ret;
    
    ret._11 = _11*mat._11 + _12*mat._21 + _13*mat._31 + _14*mat._41;
    ret._21 = _21*mat._11 + _22*mat._21 + _23*mat._31 + _24*mat._41;
    ret._31 = _31*mat._11 + _32*mat._21 + _33*mat._31 + _34*mat._41;
    ret._41 = _41*mat._11 + _42*mat._21 + _43*mat._31 + _44*mat._41;

    ret._12 = _11*mat._12 + _12*mat._22 + _13*mat._32 + _14*mat._42;
    ret._22 = _21*mat._12 + _22*mat._22 + _23*mat._32 + _24*mat._42;
    ret._32 = _31*mat._12 + _32*mat._22 + _33*mat._32 + _34*mat._42;
    ret._42 = _41*mat._12 + _42*mat._22 + _43*mat._32 + _44*mat._42;

    ret._13 = _11*mat._13 + _12*mat._23 + _13*mat._33 + _14*mat._43;
    ret._23 = _21*mat._13 + _22*mat._23 + _23*mat._33 + _24*mat._43;
    ret._33 = _31*mat._13 + _32*mat._23 + _33*mat._33 + _34*mat._43;
    ret._43 = _41*mat._13 + _42*mat._23 + _43*mat._33 + _44*mat._43;
    
    ret._14 = _11*mat._14 + _12*mat._24 + _13*mat._34 + _14*mat._44;
    ret._24 = _21*mat._14 + _22*mat._24 + _23*mat._34 + _24*mat._44;
    ret._34 = _31*mat._14 + _32*mat._24 + _33*mat._34 + _34*mat._44;
    ret._44 = _41*mat._14 + _42*mat._24 + _43*mat._34 + _44*mat._44;

    return ret;
}


//------------------------------------------------------------------------------
Matrix Matrix::operator*(float f) const
{
    Matrix ret = *this;
    ret *= f;
    return ret;
}


//------------------------------------------------------------------------------
/**
 *  Multiplies the 3x3 part of this matrix with the 3x3 part of the
 *  specified matrix. All other fields (translation...) of this matrix
 *  are passed through unchanged.
 */
Matrix Matrix::mult3x3(const Matrix & mat) const
{
    Matrix ret;
    
    ret._11 = _11*mat._11 + _12*mat._21 + _13*mat._31;
    ret._21 = _21*mat._11 + _22*mat._21 + _23*mat._31;
    ret._31 = _31*mat._11 + _32*mat._21 + _33*mat._31;
    ret._41 = _41;

    ret._12 = _11*mat._12 + _12*mat._22 + _13*mat._32;
    ret._22 = _21*mat._12 + _22*mat._22 + _23*mat._32;
    ret._32 = _31*mat._12 + _32*mat._22 + _33*mat._32;
    ret._42 = _42;

    ret._13 = _11*mat._13 + _12*mat._23 + _13*mat._33;
    ret._23 = _21*mat._13 + _22*mat._23 + _23*mat._33;
    ret._33 = _31*mat._13 + _32*mat._23 + _33*mat._33;
    ret._43 = _43;
    
    ret._14 = _14;
    ret._24 = _24;
    ret._34 = _34;
    ret._44 = _44;
    
    return ret;
}


//------------------------------------------------------------------------------
/**
 *  Multiplies the 3x3 part of this matrix with the 3x3 part of the
 *  specified matrix from the left side. All other fields
 *  (translation...) of this matrix are passed through unchanged.
 */
Matrix Matrix::mult3x3Left(const Matrix & mat) const
{
    Matrix ret;
    
    ret._11 = mat._11*_11 + mat._12*_21 + mat._13*_31;
    ret._21 = mat._21*_11 + mat._22*_21 + mat._23*_31;
    ret._31 = mat._31*_11 + mat._32*_21 + mat._33*_31;
    ret._41 = _41;

    ret._12 = mat._11*_12 + mat._12*_22 + mat._13*_32;
    ret._22 = mat._21*_12 + mat._22*_22 + mat._23*_32;
    ret._32 = mat._31*_12 + mat._32*_22 + mat._33*_32;
    ret._42 = _42;

    ret._13 = mat._11*_13 + mat._12*_23 + mat._13*_33;
    ret._23 = mat._21*_13 + mat._22*_23 + mat._23*_33;
    ret._33 = mat._31*_13 + mat._32*_23 + mat._33*_33;
    ret._43 = _43;
    
    ret._14 = _14;
    ret._24 = _24;
    ret._34 = _34;
    ret._44 = _44;
    
    return ret;
}



//------------------------------------------------------------------------------
Vector Matrix::transformPoint(const Vector & v) const
{
    return Vector(_11*v.x_ + _12*v.y_ + _13*v.z_ + _14,
                  _21*v.x_ + _22*v.y_ + _23*v.z_ + _24,
                  _31*v.x_ + _32*v.y_ + _33*v.z_ + _34);
}

//------------------------------------------------------------------------------
Vector Matrix::transformPointInv(const Vector & v) const
{
    float x = v.x_ - _14;
    float y = v.y_ - _24;
    float z = v.z_ - _34;

    return Vector(_11*x + _21*y + _31*z,
                  _12*x + _22*y + _32*z,
                  _13*x + _23*y + _33*z);
}


//------------------------------------------------------------------------------
Vector Matrix::transformVector(const Vector & v) const
{
    return Vector(_11*v.x_ + _12*v.y_ + _13*v.z_,
                  _21*v.x_ + _22*v.y_ + _23*v.z_,
                  _31*v.x_ + _32*v.y_ + _33*v.z_);
}


//------------------------------------------------------------------------------
Vector Matrix::transformVectorInv(const Vector & v) const
{
    return Vector(_11*v.x_ + _21*v.y_ + _31*v.z_,
                  _12*v.x_ + _22*v.y_ + _32*v.z_,
                  _13*v.x_ + _23*v.y_ + _33*v.z_);
}


//------------------------------------------------------------------------------
Matrix & Matrix::scale(float x, float y, float z)
{
    *(Vector*)&_11 *= x;
    *(Vector*)&_12 *= y;
    *(Vector*)&_13 *= z;
    
    return *this;
}

//------------------------------------------------------------------------------
/**
 *  Generates a rotation matrix around the x, y or z axis.
 *
 *  \param mat   Points to the matrix that should become a rotation matrix.
 *  \param angle The rotation angle in radians.
 *  \param axis  '0' is x-axis, '1' is y, '2' is z.
 *
 *  \todo check rotation direction
 */
void matRotationAxis(Matrix * mat, float angle, unsigned char axis)
{
    float cos_alpha = cosf(angle);
    float sin_alpha = sinf(angle);

    mat->_14 = 0.0f;
    mat->_24 = 0.0f;
    mat->_34 = 0.0f;
    mat->_41 = 0.0f;
    mat->_42 = 0.0f;
    mat->_43 = 0.0f;
    mat->_44 = 1.0f;
    
    switch (axis)
    {
    case 0:
        mat->_11 = 1.0f;
        mat->_12 = 0.0f;
        mat->_13 = 0.0f;
        
        mat->_21 = 0.0f;
        mat->_22 = cos_alpha;
        mat->_23 = sin_alpha;
        
        mat->_31 = 0.0f;
        mat->_32 = -sin_alpha;
        mat->_33 = cos_alpha;
        break;
    case 1:
        mat->_11 = cos_alpha;
        mat->_12 = 0.0f;
        mat->_13 = sin_alpha;
        
        mat->_21 = 0.0f;
        mat->_22 = 1.0f;
        mat->_23 = 0.0f;
        
        mat->_31 = -sin_alpha;
        mat->_32 = 0.0f;
        mat->_33 = cos_alpha;
        break;
    case 2:
        mat->_11 = cos_alpha;
        mat->_12 = sin_alpha;
        mat->_13 = 0.0f;
        
        mat->_21 = -sin_alpha;
        mat->_22 = cos_alpha;
        mat->_23 = 0.0f;
        
        mat->_31 = 0.0f;
        mat->_32 = 0.0f;
        mat->_33 = 1.0f;
        break;
    }
}


//------------------------------------------------------------------------------
Matrix * matInvertOrthogonal(Matrix * inv, const Matrix * mat)
{
    assert(inv != mat);
    
    inv->_11 = mat->_11;
    inv->_21 = mat->_12;
    inv->_31 = mat->_13;
    inv->_41 = 0.0f;
    
    inv->_12 = mat->_21;
    inv->_22 = mat->_22;
    inv->_32 = mat->_23;
    inv->_42 = 0.0f;

    inv->_13 = mat->_31;
    inv->_23 = mat->_32;
    inv->_33 = mat->_33;
    inv->_43 = 0.0f;
    
    inv->_14 = -(mat->_14*inv->_11 + mat->_24*inv->_12 + mat->_34*inv->_13);
    inv->_24 = -(mat->_14*inv->_21 + mat->_24*inv->_22 + mat->_34*inv->_23);
    inv->_34 = -(mat->_14*inv->_31 + mat->_24*inv->_32 + mat->_34*inv->_33);
    inv->_44 = 1.0f;
    
    return inv;
}


//------------------------------------------------------------------------------
/**
 *  Creates a transformation matrix which transforms into the
 *  specified position on the xz-plane with the given direction.
 *
 *  The y coordinate of Vector2d corresponds to the z-coordinate of
 *  Vector.
 *
 *  As in an OpenGL camera transform, -z is assumed to be forward, x
 *  is right and y is up, so a dir of (0,-1) corresponds to an
 *  identity rotation.
 *
 *  \param mat Will be set to the resulting matrix.
 *  \param pos The position to transform to.
 *  \param dir The direction to transform to. Does not need to be normalized.
 */
void matSetTransform(Matrix * mat, const Vector & pos, const Vector2d & dir)
{
    mat->loadIdentity();

    mat->getTranslation() = pos;

    float length = dir.length();
    if (equalsZero(length)) return;
    Vector normalized_dir = dir / length;

    mat->_11 = -normalized_dir.z_;
    mat->_31 =  normalized_dir.x_;
    mat->_13 = -normalized_dir.x_;
    mat->_33 = -normalized_dir.z_;
}




//------------------------------------------------------------------------------
void matTransformAABB(AABB * res, const Matrix * mat, const AABB * aabb)
{
    Vector new_center = aabb->getCenter() + mat->getTranslation();
    Vector half_size  = aabb->getSize() * 0.5f;

    Vector dx = half_size.x_*mat->getX();
    Vector dy = half_size.y_*mat->getY();
    Vector dz = half_size.z_*mat->getZ();
    
    *res =       new_center + dx + dy + dz;
    res->enlarge(new_center + dx + dy - dz);
    res->enlarge(new_center + dx - dy + dz);
    res->enlarge(new_center + dx - dy - dz);
    res->enlarge(new_center - dx + dy + dz);
    res->enlarge(new_center - dx + dy - dz);
    res->enlarge(new_center - dx - dy + dz);
    res->enlarge(new_center - dx - dy - dz);    
}


//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const Matrix & mat)
{
    out << "[ ";
    out << *(Vector*)&mat._11 << "; ";
    out << *(Vector*)&mat._12 << "; ";
    out << *(Vector*)&mat._13 << "; ";
    out << *(Vector*)&mat._14;
    out << " ]";

    return out;
}

//------------------------------------------------------------------------------
std::istream & operator>>(std::istream & in, Matrix & mat)
{
    std::vector<std::vector<float> > values;
    in >> values;

    mat.loadIdentity();

    switch(values.size())
    {
    case 1:
        mat._11 = (*(Vector*)&values[0]).x_;
        mat._22 = (*(Vector*)&values[0]).y_;
        mat._33 = (*(Vector*)&values[0]).z_;
        break;
    case 3:
        *(Vector*)&mat._11 = *(Vector*)&values[0][0];
        *(Vector*)&mat._12 = *(Vector*)&values[1][0];
        *(Vector*)&mat._13 = *(Vector*)&values[2][0];
        break;
    case 4:
        *(Vector*)&mat._11 = *(Vector*)&values[0][0];
        *(Vector*)&mat._12 = *(Vector*)&values[1][0];
        *(Vector*)&mat._13 = *(Vector*)&values[2][0];
        *(Vector*)&mat._14 = *(Vector*)&values[3][0];
        break;
    default:
        throw Exception("Invalid number of elements for matrix. "
                        "Specify either 3 diagonal or 3x3 or 3x4 elements.");
        break;
    }

    return in;
}



namespace serializer
{

    
//------------------------------------------------------------------------------
void putInto(Serializer & s, const Matrix & m)
{
    s.put(m._11);
    s.put(m._21);
    s.put(m._31);
    s.put(m._41);

    s.put(m._12);
    s.put(m._22);
    s.put(m._32);
    s.put(m._42);

    s.put(m._13);
    s.put(m._23);
    s.put(m._33);
    s.put(m._43);

    s.put(m._14);
    s.put(m._24);
    s.put(m._34);
    s.put(m._44);
}

//------------------------------------------------------------------------------
void getFrom(Serializer & s, Matrix & m)
{
    s.get(m._11);
    s.get(m._21);
    s.get(m._31);
    s.get(m._41);

    s.get(m._12);
    s.get(m._22);
    s.get(m._32);
    s.get(m._42);

    s.get(m._13);
    s.get(m._23);
    s.get(m._33);
    s.get(m._43);

    s.get(m._14);
    s.get(m._24);
    s.get(m._34);
    s.get(m._44);
}

} // namespace serializer
