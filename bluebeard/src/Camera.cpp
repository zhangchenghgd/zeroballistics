
#include "Camera.h"

#include <GL/glu.h>

#include "utility_Math.h"
#include "Serializer.h"
#include "Log.h"
#include "ParameterManager.h"


//------------------------------------------------------------------------------
Camera::Camera() :
    viewport_width_(100), viewport_height_(100),
    transform_(true),
    perspective_needs_update_(true),
    fov_(45.0f),
    hither_(1.0f),
    yon_(10000.0f)
{
    fov_ = s_params.get<float>("camera.fov");
    hither_ = s_params.get<float>("camera.hither");
    yon_ = s_params.get<float>("camera.yon");

    if (fov_ < 10 || fov_ > 180) fov_ = 45.0f;
    fov_ = deg2Rad(fov_);
    if (hither_ > yon_) { hither_ = 1.0f; yon_ = 10000.0f; };
}

//------------------------------------------------------------------------------
Camera::~Camera()
{
}


//------------------------------------------------------------------------------
/**
 *  Applies the camera transform to the OpenGl modelview matrix
 *  stack.
 */
void Camera::applyTransform()
{
    glLoadMatrixf(transform_.getAffineInverse());

    if (perspective_needs_update_) applyPerspectiveTransform();

    frustum_.transform(transform_);
}


//------------------------------------------------------------------------------
/**
 *  Sets the OpenGl projection matrix to the orthonormal perspective
 *  transform corresponding to the specified viewport width and height.
 *
 *  \param viewport_width  The width of the viewport in pixels.
 *  \param viewport_height The height of the viewport in pixels.
 */
void Camera::setViewportSize(unsigned width, unsigned height)
{
    glViewport(0,0, width, height);

    viewport_width_  = width;
    viewport_height_ = height;
    
    perspective_needs_update_ = true;
}

//------------------------------------------------------------------------------
void Camera::getViewportSize(unsigned & width, unsigned & height) const
{
    width  = viewport_width_;
    height = viewport_height_;
}


//------------------------------------------------------------------------------
/**
 *  \param fov Field of view in radians.
 */
void Camera::setFov(float fov)
{
    fov_ = clamp(fov, deg2Rad(10.0f), deg2Rad(170.0f));
    perspective_needs_update_ = true;
}

//------------------------------------------------------------------------------
float Camera::getFov() const
{
    return fov_;
}

//------------------------------------------------------------------------------
/**
 *  Returns the ray in world coords corresponding to the specified
 *  viewport position.
 *
 *  In essence, this applies the inverse perspective and camera
 *  transform to the point.
 *
 *  \param pos The position in viewport coordinates. x-axis is to the
 *  right, y-axis is down.
 *
 *  \return The ray corresponding to the specified point (Lefthanded
 *  OpenGl coordinates)
 */
Segment Camera::viewportToWorld(Vector2d vp_pos) const
{
    vp_pos.x_ /=  (float)viewport_width_  * 0.5f;
    vp_pos.y_ /= -(float)viewport_height_ * 0.5f;

    vp_pos += Vector2d(-1.0f, 1.0f);

    Vector dir;

    dir.z_ = -1.0f;
    dir.x_ = vp_pos.x_ * tan(fov_ * 0.5f) * ( (float)viewport_width_ / viewport_height_);
    dir.y_ = vp_pos.y_ * tan(fov_ * 0.5f);
    

    dir = transform_.transformVector(dir);
    
    return Segment(transform_.getTranslation(),
                   transform_.getTranslation() + dir);
}

//------------------------------------------------------------------------------
/**
 *  Converts the given point to the canonical view volume (view
 *  frustum maps to [-1;1]^2x[0,1])
 */
Vector Camera::worldToCanonical(const Vector & pos) const
{
    Vector trans = transform_.transformPointInv(pos);

    // Divide by z value
    float f = (trans.z_ * tan(fov_ * 0.5f));
    f = equalsZero(f) ? 0.0f : 1.0f / f;

    trans.x_ *= f * viewport_height_ / viewport_width_;
    trans.y_ *= -f;
    trans.z_ = (trans.z_ + hither_) / (hither_ - yon_);

    return trans;
}


//------------------------------------------------------------------------------
void Camera::setTransform(const Matrix & mat)
{
    transform_ = mat;
}

//------------------------------------------------------------------------------
const Matrix & Camera::getTransform() const
{
    return transform_;
}

//------------------------------------------------------------------------------
void Camera::setPos(const Vector & pos)
{
    transform_.getTranslation() = pos;
}

//------------------------------------------------------------------------------
const Vector & Camera::getPos() const
{
    return transform_.getTranslation();
}


//------------------------------------------------------------------------------
const Frustum & Camera::getFrustum() const
{
    return frustum_;
}


//------------------------------------------------------------------------------
/**
 *  get Perspective Matrix, used for OSG scene_viewer
 */
const Matrix Camera::getPerspectiveTransform()
{
    Matrix perspective;
    Matrix temp;

    float aspect = (float)viewport_width_ / viewport_height_;

    glMatrixMode(GL_PROJECTION);
    glGetFloatv( GL_PROJECTION_MATRIX, temp );

    glLoadIdentity();
    gluPerspective(rad2Deg(fov_), aspect, hither_, yon_);
    glGetFloatv( GL_PROJECTION_MATRIX, perspective );

    glLoadMatrixf(temp);

    glMatrixMode(GL_MODELVIEW);

    return perspective;
}

//------------------------------------------------------------------------------
void Camera::setHither(float hither)
{
    if (hither > yon_) return;
    hither_ = hither;
    perspective_needs_update_ = true;
}


//------------------------------------------------------------------------------
void Camera::setYon(float yon)
{
    if (hither_ > yon) return;
    yon_ = yon;
    perspective_needs_update_ = true;
}



//------------------------------------------------------------------------------
/**
 *  Sets the OpenGl projection matrix corresponding to the internal
 *  width_ and height_.
 */
void Camera::applyPerspectiveTransform()
{
    float aspect = (float)viewport_width_ / viewport_height_;
    
    glMatrixMode(GL_PROJECTION);

    glLoadIdentity();
    gluPerspective(rad2Deg(fov_), aspect, hither_, yon_);
    frustum_.create(fov_, aspect, hither_, yon_);
    
    glMatrixMode(GL_MODELVIEW);

    perspective_needs_update_ = false;
}


namespace serializer
{
    
//------------------------------------------------------------------------------
void putInto(Serializer & s, const Camera & c)
{
    s.put(c.getTransform());
}
    
//------------------------------------------------------------------------------
void getFrom(Serializer & s, Camera & c)
{
    Matrix trans;
    s.get(trans);
    c.setTransform(trans);
}

}
