/********************************************************************************
 *
 *  filename            : Bitmap.cpp
 *  author              : Muschick Christian
 *  date of creation    : 25.02.2003
 *  date of last change : 17.12.2005
 *
 *  Implementation file for the class Frustum.
 *
 *******************************************************************************/

#include "Frustum.h"

#include <limits>

#undef min
#undef max

//------------------------------------------------------------------------------
Frustum::Frustum()
{
    memset(&plane_, 0, sizeof(plane_));
}

//------------------------------------------------------------------------------
/**
 *  Sets the frustum properties.
 *
 *  \param fov          The vertical field of view angle in radians.
 *  \param aspect_ratio width / height.
 *  \param yon          The distance of the far clipping plane.
 *  \param hither       The distance of the near clipping plane.
 */
void Frustum::create(float fov, float aspect_ratio, float hither, float yon)
{
    calcPlaneNormals(fov, aspect_ratio);
    yon_    = yon;
    hither_ = hither;
}

//------------------------------------------------------------------------------
/**
 *  Transforms the frustum planes and frustum edge directions to world
 *  coordinates by applying the inverse camera transform.
 *
 *  \param mat The current inverse camera position.
 */
void Frustum::transform(const Matrix & mat)
{    
    eye_ = *(Vector*)&mat._14;


    for (unsigned i=0; i<6; ++i)
    {
        plane_[i].normal_ = mat.transformVector(plane_normal_local_[i]);
    }

    plane_[0].setPointOnPlane(eye_ - plane_[0].normal_*hither_);
    plane_[1].setPointOnPlane(eye_);
    plane_[2].setPointOnPlane(eye_);
    plane_[3].setPointOnPlane(eye_);
    plane_[4].setPointOnPlane(eye_);
    plane_[5].setPointOnPlane(eye_ + plane_[5].normal_*yon_);


    edge_[0] = Vector(-plane_normal_local_[1].z_,  plane_normal_local_[3].z_, -1.0f); // Top left
    edge_[1] = Vector(-plane_normal_local_[1].z_, -plane_normal_local_[3].z_, -1.0f); // Bottom left
    edge_[2] = Vector( plane_normal_local_[1].z_, -plane_normal_local_[3].z_, -1.0f); // Bottom right
    edge_[3] = Vector( plane_normal_local_[1].z_,  plane_normal_local_[3].z_, -1.0f); // Top right

    for (unsigned i=0; i<4; ++i)
    {
        edge_[i] = mat.transformVector(edge_[i]);
    }
}


//------------------------------------------------------------------------------
bool Frustum::isInside(const Vector & p, float radius) const
{
    for (unsigned plane=0; plane<5; ++plane)
    {
        if (plane_[plane].evalPoint(p) > radius) return false;
    }

    return true;
}

//------------------------------------------------------------------------------
/**
 *  For each frustum plane, determines the two corners of the AABB whose diagonal
 *  most closely aligns with the plane normal.
 *  If the nearest point is outside, the entire AABB is outside.
 *  If the nearest and farthest are on different sides, the box intersects,
 *  else the box is on the inside of the tested plane.
 */
CLIP_STATUS Frustum::intersect(const AABB & aabb) const
{   
    Vector farthest, nearest;
    
    for (int i=0; i<6; i++)
    {   
        if (plane_[i].normal_.x_ >= 0)
        {
            nearest.x_ = aabb.min_.x_;
            farthest.x_ = aabb.max_.x_;
        }
        else
        {
            farthest.x_ = aabb.min_.x_;
            nearest.x_ = aabb.max_.x_;
        }

        if (plane_[i].normal_.y_ >= 0)
        {
            nearest.y_ = aabb.min_.y_;
            farthest.y_ = aabb.max_.y_;
        }
        else
        {
            farthest.y_ = aabb.min_.y_;
            nearest.y_ = aabb.max_.y_;
        }

        if (plane_[i].normal_.z_ >= 0)
        {
            nearest.z_ = aabb.min_.z_;
            farthest.z_ = aabb.max_.z_;
        }
        else
        {
            farthest.z_ = aabb.min_.z_;
            nearest.z_ = aabb.max_.z_;
        }
        
        if (plane_[i].evalPoint(nearest)  > 0.0f) return CS_OUTSIDE;
        if (plane_[i].evalPoint(farthest) > 0.0f) return CS_INTERSECT;
    }

    return CS_INSIDE;
}

//------------------------------------------------------------------------------
/**
 *  Intersect the frustum with a horizontal plane of the given height.
 *
 *  First intersects the edges lines of the view frustum with the
 *  plane. If an edge doesn't intersect the plane, calculates the IP
 *  by intersecting the neighboring planes of the view frustum, the
 *  horizontal plane and the far clipping plane.
 *
 *  \param h The height of the horizontal plane.
 *
 *  \param include_camera_pos If true, the camera position is added to
 *  the resulting polygon.
 *
 *  \return An array containing the intersection points. Can be of
 *  length 0, 3, 4 or 5 if include_camera_pos is false or 0, 4,5,6 if
 *  it's true.
 */
std::vector<Vector2d> Frustum::intersectHorzPlane(float h, bool include_camera_pos) const
{    
    std::vector<Vector2d> ret;
    ret.reserve(6);
    
    unsigned intersection = 0; // Bit mask for occured intersections
    unsigned num_intersections = 0;


    // First traverse all frustum edges and calculate IP with horz
    // plane.
    for (unsigned i=0; i<4; ++i)
    {
        if (eye_.y_ < h && edge_[i].y_ > EPSILON ||
            eye_.y_ > h && edge_[i].y_ < -EPSILON)
        {
            Vector ip = Vector2d(eye_.x_ + (h - eye_.y_)*edge_[i].x_ / edge_[i].y_,
                                 eye_.z_ + (h - eye_.y_)*edge_[i].z_ / edge_[i].y_);


            // Bail if point lies farther away than the far clipping plane.
            if (plane_[5].evalPoint(ip) > 0.0f) continue;
            
            // Edge intersects horz plane, mark as intersecting and
            // calculate IP.
            intersection |= 1<<i;
            ++num_intersections;
            
            ret.push_back(ip);
        }
    }

    unsigned ind1=0, ind2=0, ind3=0, ind4=0;
    Vector tmp1, tmp2;
    switch (num_intersections)
    {
    case 4:
        // If all edges intersected the plane there's nothing left to do.
        break;
    case 0:
        // Bail if no intersection occured.
        break;
        
        // One two three edges intersecting: The other IPs are
        // obtained by intersecting with the neighboring frustum
        // planes and the far plane.
    case 1:
        ind1 = ld(intersection);
        
        ret.push_back(intersectHorzPlane(h, ind1, (ind1+1) % 4));
        ret.push_back(intersectHorzPlane(h, ind1, (ind1+3) % 4));

        break;
    case 2:
        while (!(intersection & (1<<ind1))) ++ind1;
        ind2 = ind1+1;
        if (!(intersection & (1<<ind2)))
        {
            std::swap(ret[0], ret[1]);
            
            ind1 = 3;
            ind2 = 0;
            ind3 = 1;
            ind4 = 2;
        } else
        {
            ind3 = ind2 == 3 ? 0:ind2+1;
            ind4 = ind3 == 3 ? 0:ind3+1;
        }
        
        assert (intersection & (1 << ind2));

        ret.push_back(intersectHorzPlane(h, ind2, ind3));
        ret.push_back(intersectHorzPlane(h, ind1, ind4));

        break;
    case 3:
        while (!(intersection & (1<<ind1))) ++ind1;
        ind2 = ind1+1;
        ind3 = ind1+2;

        if (!(intersection & (1<<ind2)))
        {
            std::swap(ret[0], ret[1]);
            std::swap(ret[2], ret[1]);
            
            ind1 = 2;
            ind2 = 3;
            ind3 = 0;
            ind4 = 1;
        } else if (!(intersection & (1<<ind3)))
        {
            std::swap(ret[0], ret[2]);
            std::swap(ret[1], ret[2]);
            
            ind1 = 3;
            ind2 = 0;
            ind3 = 1;
            ind4 = 2;
        } else
        {
            ind4 = ind3 == 3 ? 0:ind3+1;
        }
        
        assert(intersection & (1 << ind3) &&
               (intersection & (1 << ind2) ||
                intersection & (1 << ind4) ));

        ret.push_back(intersectHorzPlane(h, ind3, ind4));
        ret.push_back(intersectHorzPlane(h, ind1, ind4));

                
        break;
    default:
        assert(false);
    }

    // If the camera position is to be included in the polygon, we
    // need to add it to the convex hull.
    if (include_camera_pos && !ret.empty())
    {
        // Find the two ponits that are nearest to the camera pos
        Vector2d eye2d = eye_;

        float min_dist1 = (eye2d - ret[0]).lengthSqr();
        float min_dist2 = std::numeric_limits<float>::max();
        unsigned min_index1 = 0;
        unsigned min_index2 = 0;
        
        for (unsigned i=1;i<ret.size(); ++i)
        {
            float cur_dist = (eye2d - ret[i]).lengthSqr();
            
            if (cur_dist <= min_dist1)
            {
                min_dist2  = min_dist1;
                min_index2 = min_index1;
                min_dist1  = cur_dist;
                min_index1 = i;
            } else if (cur_dist < min_dist2)
            {
                min_dist2  = cur_dist;
                min_index2 = i;
            }
        }
        
        if (ret.size() == 3)
        {
            // Simply replace the point nearest to the camera with the eye position.
            ret[min_index1] = eye2d;
        } else
        {
            if (min_index1 == min_index2+1 ||
                (min_index1 == 0 && min_index2 != 1)) std::swap(min_index1, min_index2);

            assert(min_index1+1 == min_index2 ||
                   (min_index1 == ret.size()-1 && min_index2 == 0));

            // Determine order of polygon
            Vector2d n = (ret[min_index2]-ret[min_index1]).tilt();
            Vector2d vp = ret[(min_index1+2)% ret.size()]-ret[min_index1];
            bool pos_in = vecDot(&n, &vp) > 0.0f;

            // Only continue if eye point doesn't lie inside polygon
            vp = eye2d - ret[min_index1];
            if ((vecDot(&n, &vp) > 0.0f) != pos_in)
            {
                // Add the camera position between the two closest points
                if (min_index2 == 0)
                {
                    ret.push_back(eye2d);
                } else
                {
                    ret.insert(ret.begin()+min_index2, eye2d);
                }
            }
        }
    }

    return ret;
}


//------------------------------------------------------------------------------
const Vector & Frustum::getEyePos() const
{
    return eye_;
}


//------------------------------------------------------------------------------
/**
 *  Returns a pointer to the six planes comprising the view
 *  frustum. \see plane_ for order.
 */
const Plane * Frustum::getPlane() const
{
    return plane_;
}

//------------------------------------------------------------------------------
/**
 *  \see edge_
 */ 
const Vector * Frustum::getEdgeDir() const
{
    return edge_;
}
    

//------------------------------------------------------------------------------
float Frustum::getHither() const
{
    return hither_;
}

//------------------------------------------------------------------------------
float Frustum::getYon() const
{
    return yon_;
}


//------------------------------------------------------------------------------
/**
 *  Calculates the normals of the frustum planes in camera space.
 */
void Frustum::calcPlaneNormals(float fov, float aspect_ratio)
{
    float tan_half_fov = tanf(fov * 0.5f);
    
    plane_normal_local_[0] = Vector( 0.0, 0.0f, 1.0f);                      // Near
    plane_normal_local_[1] = Vector(-1.0, 0.0f, tan_half_fov*aspect_ratio); // Left
    plane_normal_local_[2] = Vector( 1.0, 0.0f, tan_half_fov*aspect_ratio); // Right
    plane_normal_local_[3] = Vector( 0.0, 1.0f, tan_half_fov);              // Top
    plane_normal_local_[4] = Vector( 0.0,-1.0f, tan_half_fov);              // Bottom
    plane_normal_local_[5] = Vector( 0.0, 0.0f, -1.0f);                     // Far
}

//------------------------------------------------------------------------------
/**
 *  Calculates the intersection point of a horizontal plane with
 *  height h, the far clipping plane and the frustum plane spanned
 *  between the given edges.
 *
 *  \param h The height of the horizontal plane y=h.
 *  \param e1 The index of the first edge.
 *  \param e2 The index of the second edge.
 *
 *  \return The intersection point.
 */
Vector2d Frustum::intersectHorzPlane(float h, unsigned e1, unsigned e2) const
{
    Vector p1 = eye_ + edge_[e1]*yon_;
    Vector p2 = eye_ + edge_[e2]*yon_;

    float s = (h-p1.y_) / (p2.y_-p1.y_);
    
    return (Vector2d)p1 + s*((Vector2d)p2-p1);
}
