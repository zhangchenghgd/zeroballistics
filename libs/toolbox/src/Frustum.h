/*******************************************************************************
 *
 *  filename            : Frustum.h
 *  author              : Muschick Christian
 *  date of creation    : 25.02.2003
 *  date of last change : 17.12.2005
 *
 *
 *******************************************************************************/

#ifndef RACING_FRUSTUM_INCLUDED
#define RACING_FRUSTUM_INCLUDED


#include <cmath>

#include "Matrix.h"
#include "Vector.h"
#include "Plane.h"
#include "Utils.h"
#include "Geometry.h"

//------------------------------------------------------------------------------
enum CLIP_STATUS
{
    CS_INSIDE,
    CS_OUTSIDE,
    CS_INTERSECT
};

//------------------------------------------------------------------------------
enum FRUSTUM_PLANE
{
    FP_NEAR   = 0,
    FP_LEFT   = 1,
    FP_RIGHT  = 2,
    FP_TOP    = 3,
    FP_BOTTOM = 4,
    FP_FAR    = 5
};

//------------------------------------------------------------------------------
enum FRUSTUM_EDGE
{
    FE_TOP_LEFT,
    FE_BOTTOM_LEFT,
    FE_BOTTOM_RIGHT,
    FE_TOP_RIGHT
};

//------------------------------------------------------------------------------
class Frustum
{
 public:
    Frustum();

    void create(float fov, float aspect_ratio, float hither, float yon);
    void transform(const Matrix & mat);

    bool isInside(const Vector & p, float radius) const;
     
    CLIP_STATUS intersect(const AABB & aabb) const;
    std::vector<Vector2d> intersectHorzPlane(float h, bool include_camera_pos = false) const;
    
    const Vector & getEyePos() const;
    
    const Plane * getPlane() const;

    const Vector * getEdgeDir() const;
    
    float getHither() const;
    float getYon() const;
    
 protected:
    void calcPlaneNormals(float fov, float aspect_ratio);

    Vector2d intersectHorzPlane(float h, unsigned e1, unsigned e2) const;
    
    Vector plane_normal_local_[6]; ///< The six plane normals in local
                                   ///  coordinates. Normals point
                                   ///  outside our view
                                   ///  frustum. Since 4 of the
                                   ///  frustum planes intersect the
                                   ///  origin, we need only the
                                   ///  normal vectors. Order: near,
                                   ///  left, right, top, bottom, far.
    
    Plane plane_[6];               ///< The six planes comprising the
                                   ///  frustum in world
                                   ///  coordinates. These are
                                   ///  calculated from the local
                                   ///  normals using the camera
                                   ///  transform in
                                   ///  transform(). Order: near,
                                   ///  left, right, top, bottom,
                                   ///  far. Plane normals point
                                   ///  outside the frustum.

    Vector eye_;                   ///< The eye point in world coordinates.
    Vector edge_[4];               ///< The frustum edge directions in
                                   ///world coordinates. The length is
                                   ///chosen with z=1 in local
                                   ///coordinates. Order: Top left,
                                   ///Bottom left, Bottom right, Top
                                   ///right.
    

    float yon_;                    ///< Distance to the far clipping plane.
    float hither_;                 ///< Distance to the near clipping plane.
};

#endif // #ifndef RACING_FRUSTUM_INCLUDED
