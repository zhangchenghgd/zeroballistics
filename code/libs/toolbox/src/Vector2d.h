
#ifndef STUNTS_VECTOR2D_INCLUDED
#define STUNTS_VECTOR2D_INCLUDED


#include <iomanip>

#include <cmath>

class Vector;
class Matrix;

//------------------------------------------------------------------------------
class Vector2d
{
 public:
    Vector2d();
    Vector2d(float x, float y);
    Vector2d(float x, float y, float z);

    float length() const;
    float lengthSqr() const;
    void normalize();
    void safeNormalize();

    
    
    Vector2d operator-() const;

    Vector2d operator+(const Vector2d & v2) const;
    Vector2d operator-(const Vector2d & v2) const;
    Vector2d operator*(float f) const;
    Vector2d operator/(float f) const;

    Vector2d & operator+=(const Vector2d & v2);
    Vector2d & operator-=(const Vector2d & v2);
    Vector2d & operator*=(float f);
    Vector2d & operator/=(float f);

    float operator*(const Vector2d & v2) const;

    
    Vector2d tilt() const;

    operator Vector() const;
    
    float x_;
    float y_;
};


float vecCross(const Vector2d * v1, const Vector2d * v2);
float vecCosAlpha(const Vector2d * v1, const Vector2d * v2);
float vecDot(const Vector2d * v1, const Vector2d * v2);

Vector2d operator*(float f, Vector2d v);


std::ostream & operator<<(std::ostream & out, const Vector2d & v);
std::istream & operator>>(std::istream & in, Vector2d & v);

namespace serializer
{
class Serializer;
    
void putInto(Serializer & s, const Vector2d & v);    
void getFrom(Serializer & s, Vector2d & v);

}


#endif // #ifndef STUNTS_VECTOR2D_INCLUDED
