
#ifndef RACING_CAMERA_INCLUDED
#define RACING_CAMERA_INCLUDED

#include <iostream>


#include "Matrix.h"
#include "Datatypes.h"
#include "Geometry.h"
#include "Frustum.h"

namespace serializer
{
    class Serializer;
}


//------------------------------------------------------------------------------
class Camera
{
 public:
    Camera();
    virtual ~Camera();

    void applyTransform();

    void setViewportSize(unsigned width, unsigned height);
    void getViewportSize(unsigned & width, unsigned & height) const;

    void setHither(float hither);
    void setYon(float yon);

    void setFov(float fov);
    float getFov() const;

    Segment viewportToWorld(Vector2d pos) const;
    Vector worldToCanonical(const Vector & pos) const;


    void setTransform(const Matrix & mat);
    const Matrix & getTransform() const;

    void setPos(const Vector & pos);
    const Vector & getPos() const;

    const Frustum & getFrustum() const;

    const Matrix getPerspectiveTransform();

        
 private:

    void applyPerspectiveTransform();
    
    uint32_t viewport_width_;
    uint32_t viewport_height_;
    
    Matrix transform_;        ///< The current camera transformation matrix.
    
    mutable bool perspective_needs_update_;
    
    float fov_;    ///< Field of view in radians
    float hither_; ///< Near clipping plane
    float yon_;    ///< Far clipping plane

    Frustum frustum_;
};

namespace serializer
{
    
void putInto(Serializer & s, const Camera & v);
    
void getFrom(Serializer & s, Camera & v);

}


#endif // #ifndef RACING_CAMERA_INCLUDED
