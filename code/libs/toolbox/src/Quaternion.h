

#ifndef LIBSTUNTS_QUATERNION_INCLUDED
#define LIBSTUNTS_QUATERNION_INCLUDED


#include "Matrix.h"


//------------------------------------------------------------------------------
class Quaternion
{
 public:
    Quaternion();
    Quaternion(float x, float y, float z, float w);
    Quaternion(const Matrix & mat);

    Quaternion operator-(const Quaternion & other) const;
    
    float length() const;
    float lengthSqr() const;
    void normalize();
    
    void toMatrix(Matrix & mat) const;
    


    float x_;  ///< Vector x.
    float y_;  ///< Vector y.
    float z_;  ///< Vector z.
    float w_;  ///< Scalar component.
};



#endif
