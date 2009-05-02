
#ifndef LIB_MATRIX_INCLUDED
#define LIB_MATRIX_INCLUDED

#include "Vector.h"
#include "Geometry.h"
#include "Vector2d.h"
#include "Utils.h"

//------------------------------------------------------------------------------
/**
 *  Matrix is stored in column-major order.
 */
class Matrix
{
 public:
    Matrix();
    explicit Matrix(bool initialize);
    explicit Matrix(const double * pd);
    explicit Matrix(const float * pf);
    Matrix & operator=(const double * pd);
    Matrix & operator=(const float * pf);
    Matrix( float32_t r1c1, float32_t r1c2, float32_t r1c3, float32_t r1c4,
            float32_t r2c1, float32_t r2c2, float32_t r2c3, float32_t r2c4,
            float32_t r3c1, float32_t r3c2, float32_t r3c3, float32_t r3c4,
            float32_t r4c1, float32_t r4c2, float32_t r4c3, float32_t r4c4);

    void loadIdentity();
    void loadCanonicalRotation(float angle, unsigned char axis);
    void loadRotationVector(float angle, const Vector & v);
    void loadOrientation(const Vector & dir, const Vector & up);
    void loadOrientationPart(float yaw, float pitch);
    void loadScaling(float x, float y, float z);

    void getEuler(float & x, float & y, float & z) const;
    
    void orthonormalize();

    void invertAffine();
    Matrix getAffineInverse() const;

    void invert3x3();
    Matrix get3x3Inverse() const;

    Matrix getTranspose3x3() const;
    Matrix getTranspose4x4() const;
    
    float determinant() const;
    
    Vector & getX() const;
    Vector & getY() const;
    Vector & getZ() const;
    Vector & getTranslation() const;
    
    Matrix & operator+=(const Matrix & mat);
    Matrix & operator*=(const Matrix & mat);
    Matrix & operator*=(float f);
    Matrix & operator/=(float f);

    Matrix operator-(const Matrix & mat) const;
    Matrix operator*(const Matrix & mat) const;
    Matrix operator*(float f) const;

    Matrix mult3x3    (const Matrix & mat) const;    
    Matrix mult3x3Left(const Matrix & mat) const;    
    
    Vector transformPoint    (const Vector & v) const;
    Vector transformPointInv (const Vector & v) const;
    Vector transformVector   (const Vector & v) const;
    Vector transformVectorInv(const Vector & v) const;
    
    Matrix & scale(float x, float y, float z);

    /// Used for glGetFloatv etc.
    operator       float32_t* () const {return (      float32_t*) this;}
    /// Used for glLoadMatrixf etc.
    operator const float32_t* () const {return (const float32_t*) this;}
    
    float32_t _11; ///< Column 1
    float32_t _21;
    float32_t _31;
    float32_t _41;

    float32_t _12; ///< Column 2
    float32_t _22;
    float32_t _32;
    float32_t _42;

    float32_t _13; ///< Column 3
    float32_t _23;
    float32_t _33;
    float32_t _43;

    float32_t _14; ///< Translation x
    float32_t _24; ///< Translation y
    float32_t _34; ///< Translation z
    float32_t _44;

};



void matTransformAABB(AABB * res, const Matrix * mat, const AABB * aabb);





//------------------------------------------------------------------------------
/**
 *  Calculates matrix*vector and stores the result in res.
 *  Ignores any translation.
 */
inline Vector2d * matTransformVector(Vector2d * res, const Matrix * matrix, const Vector2d * v)
{
    float x = v->x_;
    float y = v->y_;

    res->x_ = matrix->_11*x + matrix->_13*y;
    res->y_ = matrix->_31*x + matrix->_33*y;

    return res;
}




//------------------------------------------------------------------------------
/**
 *  Calculates matrix*vector and stores the result in res.
 *  Translations are taken into account.
 */
inline Vector2d * matTransformPoint(Vector2d * res, const Matrix * matrix, const Vector2d * point)
{
    float x = point->x_;
    float y = point->y_;

    res->x_ = matrix->_11*x + matrix->_13*y + matrix->_14;
    res->y_ = matrix->_31*x + matrix->_33*y + matrix->_34;

    return res;
}


//------------------------------------------------------------------------------
inline Segment * matTransformSegment(Segment * res, const Matrix * matrix, const Segment * seg)
{
    res->p_ = matrix->transformPoint(seg->p_);
    res->d_ = matrix->transformVector(seg->d_);

    return res;
}

//------------------------------------------------------------------------------
/**
 *  Matrix must be invertible by transposing it (just rotations).
 */
inline Vector2d * matTransformVectorInv(Vector2d * res, const Matrix * matrix, const Vector2d * v)
{
    float x = v->x_;
    float y = v->y_;

    res->x_ = matrix->_11*x + matrix->_31*y;
    res->y_ = matrix->_13*x + matrix->_33*y;

    return res;
}


//------------------------------------------------------------------------------
/**
 *  Matrix must be invertible by transposing it (just rotations).
 */
inline Vector2d * matTransformPointInv(Vector2d * res, const Matrix * matrix, const Vector2d * point)
{
    float x = point->x_ - matrix->_14;
    float y = point->y_ - matrix->_34;

    res->x_ = matrix->_11*x + matrix->_31*y;
    res->y_ = matrix->_13*x + matrix->_33*y;

    return res;
}


//------------------------------------------------------------------------------
/**
 *  Matrix must be invertible by transposing it (just rotations).
 */
inline Segment * matTransformSegmentInv(Segment * res, const Matrix * matrix, const Segment * seg)
{
    res->p_ = matrix->transformPointInv (seg->p_);
    res->d_ = matrix->transformVectorInv(seg->d_);
    return res;
}


//------------------------------------------------------------------------------
/**
 *  Calculates a differentiation matrix omega where dR/dt = omega * R.
 *  (physics for game developers p.226)
 */
inline Matrix * matDifferentiationMatrix(Matrix * mat, const Vector * ang_velocity)
{
    mat->_11 = 0.0f;
    mat->_21 =  ang_velocity->z_;
    mat->_31 = -ang_velocity->y_;
    mat->_41 = 0.0f;

    mat->_12 = -ang_velocity->z_;
    mat->_22 = 0.0f;
    mat->_32 =  ang_velocity->x_;
    mat->_42 = 0.0f;

    mat->_13 =  ang_velocity->y_;
    mat->_23 = -ang_velocity->x_;
    mat->_33 = 0.0f;
    mat->_43 = 0.0f;

    mat->_14 = 0.0f;
    mat->_24 = 0.0f;
    mat->_34 = 0.0f;
    mat->_44 = 0.0f;

    return mat;
}

//------------------------------------------------------------------------------
/**
 *  mat must be of the form A * diag(sx, sy, sz) where A is an
 *  orthogonal matrix. The scale matrix is stored in s, the matrix
 *  stripped from scaling factors in res.
 */
inline void matExtractScaleFactors(Matrix * res, Matrix * s, const Matrix * mat)
{
    s->loadIdentity();

    s->_11 = mat->getX().length();
    s->_22 = mat->getY().length();
    s->_33 = mat->getZ().length();

    *res = *mat;
    res->getX() /= s->_11;
    res->getY() /= s->_22;
    res->getZ() /= s->_33;
}



std::ostream & operator<<(std::ostream & out, const Matrix & mat);
std::istream & operator>>(std::istream & in, Matrix & mat);


namespace serializer
{
    
void putInto(Serializer & s, const Matrix & m);
    
void getFrom(Serializer & s, Matrix & m);

}


#endif // #ifndef LIB_MATRIX_INCLUDED
