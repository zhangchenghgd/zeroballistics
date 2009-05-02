/*******************************************************************************
 *
 *  Copyright 2004 Muschick Christian
 *  
 *  This file is part of Lear.
 *  
 *  Lear is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  Lear is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with Lear; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  -----------------------------------------------------------------------------
 *
 *  filename            : Geometry.cpp
 *  author              : Muschick Christian
 *  date of creation    : 02.09.2003
 *  date of last change : 07.01.2004
 *
 *******************************************************************************/

#include "Geometry.h"


#include "utility_Math.h"
#include "Vector2d.h"
#include "Serializer.h"


//------------------------------------------------------------------------------
AABB::AABB() : min_(Vector(0.0f, 0.0f, 0.0f)),
               max_(Vector(0.0f, 0.0f, 0.0f))
               
{
}

//------------------------------------------------------------------------------
AABB::AABB(const Vector & min, const Vector & max) : min_(min), max_(max)
{
}

//------------------------------------------------------------------------------
AABB & AABB::operator=(const Vector & p)
{
    min_ = p;
    max_ = p;
    
    return *this;
}

//------------------------------------------------------------------------------
/**
 *  Enlarges the AABB to include the specified point.
 */
void AABB::enlarge(const Vector & p)
{
    if      (p.x_ > max_.x_) max_.x_ = p.x_;
    else if (p.x_ < min_.x_) min_.x_ = p.x_;

    if      (p.y_ > max_.y_) max_.y_ = p.y_;
    else if (p.y_ < min_.y_) min_.y_ = p.y_;

    if      (p.z_ > max_.z_) max_.z_ = p.z_;
    else if (p.z_ < min_.z_) min_.z_ = p.z_;
}

//------------------------------------------------------------------------------
void AABB::merge(const AABB & aabb)
{
    enlarge(aabb.max_);
    enlarge(aabb.min_);
}


//------------------------------------------------------------------------------
Vector AABB::getSize() const
{
    return max_ - min_;
}


//------------------------------------------------------------------------------
Vector AABB::getCenter() const
{
    return (max_ + min_) * 0.5f;
}



//------------------------------------------------------------------------------
Box2d::Box2d()
{
}

//------------------------------------------------------------------------------
Box2d::Box2d(Vector2d top_left, Vector2d bottom_right) :
    top_left_(top_left), bottom_right_(bottom_right)
{
}


//------------------------------------------------------------------------------
Box2d::Box2d(Vector2d v) :
    top_left_(v), bottom_right_(v)
{
}

//------------------------------------------------------------------------------
Box2d::~Box2d()
{
}

//------------------------------------------------------------------------------
Box2d & Box2d::operator=(Vector2d v)
{
    top_left_ = v;
    bottom_right_ = v;

    return *this;
}

//------------------------------------------------------------------------------
bool Box2d::isInside(Vector2d v)
{
    return top_left_.x_ < v.x_ && bottom_right_.x_ > v.x_ &&
        top_left_.y_ > v.y_ && bottom_right_.y_ < v.y_;
}

//------------------------------------------------------------------------------
void Box2d::extendBy(Vector2d v)
{
    if (v.x_ > bottom_right_.x_) bottom_right_.x_ = v.x_;
    else if (v.x_ < top_left_.x_) top_left_.x_ = v.x_;

    if (v.y_ < bottom_right_.y_) bottom_right_.y_ = v.y_;
    else if (v.y_ > top_left_.y_) top_left_.y_ = v.y_;
}


//------------------------------------------------------------------------------
Line2d::Line2d() : n_(Vector2d(0,0)), c_(0)
{
}

//------------------------------------------------------------------------------
Line2d::Line2d(Vector2d n, Vector2d p) : n_(n)
{
    n_.normalize();
    c_ = -vecDot(&n_, &p);
}


//------------------------------------------------------------------------------
float Line2d::evalPoint(Vector2d v) const
{
    return vecDot(&v, &n_) + c_;
}

//------------------------------------------------------------------------------
Vector2d Line2d::clip(Vector2d p1, Vector2d p2) const
{
    float d1 = evalPoint(p1);
    float d2 = evalPoint(p2);

    return p1 + d1/(d1-d2)*(p2-p1);
}



//------------------------------------------------------------------------------
Segment::Segment()
{
}

//------------------------------------------------------------------------------
Segment::Segment(const Vector & p1, const Vector & p2) : p_(p1), d_(p2 - p1)
{
}


//------------------------------------------------------------------------------
void Segment::set(const Vector & p1, const Vector & p2)
{
    p_ = p1;
    d_ = p2-p1;
}

//------------------------------------------------------------------------------
void Segment::setDir(const Vector & p, const Vector & dir)
{
    p_ = p;
    d_ = dir;
}

//------------------------------------------------------------------------------
/**
 *  Returns the cosine of the angle between the two segment's
 *  directions.
 */
float segCosAngle(const Segment * seg1, const Segment * seg2)
{
    return vecDot(&seg1->d_, &seg2->d_) / (seg1->d_.length() * seg2->d_.length());
}

//------------------------------------------------------------------------------
/**
 *  Calculates the two closest points lying on seg1 and seg2.
 *  See 3D Game Engine Design p.41 ff.
 */
void segGetClosestPoints(Vector * x0, Vector * x1, const Segment * seg1, const Segment * seg2)
{
    float a, b, c, d, e, f, 
          tmp, det, s, t;

    Vector p1p0 = seg1->p_ - seg2->p_;
    
    a =  vecDot(&seg1->d_, &seg1->d_);
    b = -vecDot(&seg1->d_, &seg2->d_);
    c =  vecDot(&seg2->d_, &seg2->d_);
    d =  vecDot(&seg1->d_, &p1p0);
    e = -vecDot(&seg2->d_, &p1p0);
    f =  vecDot(&p1p0,     &p1p0);

    det = abs(a*c - b*b);

    // parallel ?
    if (det*det < EPSILON * EPSILON * abs(a*b))
    {
        if ( b > 0.0f )
        {
            // direction vectors form an obtuse angle
            if ( d >= 0.0f )
            {
                s = 0.0f;
                t = 0.0f;
            }
            else if ( -d <= a )
            {
                s = -d/a;
                t = 0.0f;
            }
            else
            {
                s = 1.0f;
                tmp = a+d;
                if ( -tmp >= b )
                {
                    t = 1.0f;
                }
                else
                {
                    t = -tmp/b;
                }
            }
        }
        else
        {
            // direction vectors form an acute angle
            if ( -d >= a )
            {
                s = 1.0f;
                t = 0.0f;
            }
            else if ( d <= 0.0f )
            {
                s = -d/a;
                t = 0.0f;
            }
            else
            {
                s = 0.0f;
                if ( d >= -b )
                {
                    t = 1.0f;
                }
                else
                {
                    t = -d/b;
                }
            }
        }
    } else // not parallel
    {
        s = (b*e - c*d);
        t = (b*d - a*e);

        if (s >= 0.0f)
        {
            if (s <= det)
            {
                if (t >= 0.0f)
                {
                    if (t <= det)
                    {
                        // region 0
                        float inv_det = 1.0f / det;
                        s *= inv_det;
                        t *= inv_det;
                    } else
                    {
                        // region 3 (top)
                        t = 1.0f;
                        tmp = b+d;
                        if ( tmp >= 0.0f )
                        {
                            s = 0.0f;
                        }
                        else if ( -tmp >= a )
                        {
                            s = 1.0f;
                        }
                        else
                        {
                            s = -tmp/a;
                        }
                    }
                } else
                {
                    // region 7 (bottom)
                    t = 0.0f;
                    if ( d >= 0.0f )
                    {
                        s = 0.0f;
                    }
                    else if ( -d >= a )
                    {
                        s = 1.0f;
                    }
                    else
                    {
                        s = -d/a;
                    }
                }
            } else
            {
                if (t >= 0.0f)
                {
                    if (t <= det)
                    {
                        // region 1 (right)
                        s = 1.0f;
                        tmp = b+e;
                        if (tmp > 0.0f)
                        {
                            t = 0.0f;
                        } else if ( -tmp >= c)
                        {
                            t = 1.0f;
                        } else
                        {
                            t = -tmp/c;
                        }

                    } else
                    {
                        // region 2 (top right)
                        tmp = b+d;
                        if ( -tmp <= a)
                        {
                            t = 1.0f;
                            if (tmp >= 0.0f)
                            {
                                s = 0.0f;
                            } else
                            {
                                s = -tmp/a;
                            }
                        } else
                        {
                            s = 1.0f;
                            tmp = b+e;
                            if ( tmp >= 0.0f)
                            {
                                t = 0.0f;
                            } else if ( -tmp >= c)
                            {
                                t = 1.0f;
                            } else
                            {
                                t = -tmp / c;
                            }
                        }
                    }
                } else
                {
                    // region 8 (bottom right)
                    if ( -d < a )
                    {
                        t = 0.0f;
                        if ( d >= 0.0f )
                        {
                            s = 0.0f;
                        }
                        else
                        {
                            s = -d/a;
                        }
                    }
                    else
                    {
                        s = 1.0f;
                        tmp = b+e;
                        if ( tmp >= 0.0f )
                        {
                            t = 0.0f;
                        }
                        else if ( -tmp >= c )
                        {
                            t = 1.0f;
                        }
                        else
                        {
                            t = -tmp/c;
                        }
                    }
                }
            }
        } else
        {
            if (t >= 0.0f)
            { 
                if (t <= det)
                {
                    // region 5 (let)
                    s = 0.0f;
                    if ( e >= 0.0f )
                    {
                        t = 0.0f;
                    }
                    else if ( -e >= c )
                    {
                        t = 1.0f;
                    }
                    else
                    {
                        t = -e/c;
                    }
                } else
                {
                    // region 4 (top let)
                    tmp = b+d;
                    if ( tmp < 0.0f )
                    {
                        t = 1.0f;
                        if ( -tmp >= a )
                        {
                            s = 1.0f;
                        }
                        else
                        {
                            s = -tmp/a;
                        }
                    }
                    else
                    {
                        s = 0.0f;
                        if ( e >= 0.0f )
                        {
                            t = 0.0f;
                        }
                        else if ( -e >= c )
                        {
                            t = 1.0f;
                        }
                        else
                        {
                            t = -e/c;
                        }
                    }
                }
            } else
            {
                // region 6 (bottom let)
                if ( d < 0.0f )
                {
                    t = 0.0f;
                    if ( -d >= a )
                    {
                        s = 1.0f;
                    }
                    else
                    {
                        s = -d/a;
                    }
                }
                else
                {
                    s = 0.0f;
                    if ( e >= 0.0f )
                    {
                        t = 0.0f;
                    }
                    else if ( -e >= c )
                    {
                        t = 1.0f;
                    }
                    else
                    {
                        t = -e/c;
                    }
                }
            }
        }
    }

    assert(s >= 0.0f && s <= 1.0f);
    assert(t >= 0.0f && t <= 1.0f);
    
    *x0 = seg1->p_ + s * seg1->d_;
    *x1 = seg2->p_ + t * seg2->d_;
}

//------------------------------------------------------------------------------
/** 
 *  Gets the point on the segment closest to the specified point p.
 *  See 3D Game Engine Design p.38.
 */
void segGetClosestPoint(Vector * x, const Vector * p, const Segment * seg)
{
    Vector pp = *p - seg->p_;
    float t = vecDot(&seg->d_, &pp);

    if ( t > 0.0f)
    {
        float dd = seg->d_.lengthSqr();
        if (t > dd)
        {
            *x = seg->p_ + seg->d_;
        } else
        {
            *x = seg->p_ + t/dd * seg->d_;
        }
    } else
    {
        *x = seg->p_;
    }
}


//------------------------------------------------------------------------------
/**
 *  Calculates the point on the segment closest to the plane.
 */
void segGetClosestPoint(Vector * x, const Plane * plane, const Segment * seg)
{
    float d1 = plane->evalPoint(seg->p_);
    float d2 = plane->evalPoint(seg->p_ + seg->d_);

    if (sign(d1) == sign(d2))
    {
        *x = abs(d1) < abs(d2) ? seg->p_ : seg->p_ + seg->d_;
    } else
    {
        float f = abs(d1)/(abs(d1)+abs(d2));
        assert(f >= 0.0f);
        assert(f <= 1.0f);
        *x = seg->p_ + f * seg->d_;
    }
}


//------------------------------------------------------------------------------
/**
 *  Calculates the distance of the given point to the ray given by
 *  x = p_ + s*d_, s e [0, oo].
 */
float segDistanceRay(const Segment * seg, const Vector * v)
{
    Vector pv = *v - seg->p_;

    // return dist to endpoint if v lies "behind" the ray
    if (vecDot(&pv, &seg->d_) < 0.0f) return pv.length();

    Vector q = seg->p_ + seg->d_;

    Vector pq = q - seg->p_;

    Vector cross;
    vecCross(&cross, &pv, &pq);
    
    return cross.length() / pq.length();
}


//------------------------------------------------------------------------------
/**
 *  Helper function for triOverlapsSegment. See Real Time Rendering
 *  p. 336.
 */
bool triSegmentsIntersect(const Vector2d & o1, const Vector2d & d1,
                          const Vector2d & o2, const Vector2d & d2)
{ 
    float denom_s = d1 * d2.tilt();
    if (equalsZero(denom_s)) return false;
    
    float s = (o2 - o1)*d2.tilt() / denom_s;
    if (s < EPSILON || s > 1.0f - EPSILON) return false;

    float denom_t = d2 * d1.tilt();
    if (equalsZero(denom_t)) return false;
    
    float t = (o1 - o2)*d1.tilt() / denom_t;
    if (t < EPSILON || t > 1.0f - EPSILON) return false;

    return true;
}


//------------------------------------------------------------------------------
/**
 *  Helper function for triIntersect.
 *
 *  \todo optimize
 */
bool triOverlapsSegment(const Vector2d & u0, const Vector2d & u1, const Vector2d & u2,
                        const Vector2d & p, const Vector2d & q)
{
    Vector2d d0 = u1 - u0,
             d1 = u2 - u1,
             d2 = u0 - u2,
             e  = q - p;
    
    int u2_side = signZ(d0.tilt() * (u2 - u0));
    
    assert(u2_side != 0 || !"degenerate triangle");
    
    int p_side0 = signZ(d0.tilt() * (p - u0));
    int p_side1 = signZ(d1.tilt() * (p - u1));
    int p_side2 = signZ(d2.tilt() * (p - u2));

    // if p is inside triangle, return overlap
    if (p_side0 == p_side1 && p_side0 == p_side2) return true;

    int q_side0 = signZ(d0.tilt() * (q - u0));
    int q_side1 = signZ(d1.tilt() * (q - u1));
    int q_side2 = signZ(d2.tilt() * (q - u2));
    
    // if q is inside triangle, return overlap
    if (q_side0 == q_side1 && q_side0 == q_side2) return true;

    // bail if both points outside the same edge
    if (p_side0 != u2_side && q_side0 != u2_side) return false;
    if (p_side1 != u2_side && q_side1 != u2_side) return false;
    if (p_side2 != u2_side && q_side2 != u2_side) return false;

    // now check each triangle segment for intersection
    if (triSegmentsIntersect(u0, d0, p, e)) return true;
    if (triSegmentsIntersect(u1, d1, p, e)) return true;
    if (triSegmentsIntersect(u2, d2, p, e)) return true;
    
    return false;
}


//------------------------------------------------------------------------------
/**
 *  Returns whether the two specified triangles intersect using Erit's
 *  method (Real-Time Rendering p. 317).
 *
 *  \todo handle coplanar case
 */
bool triIntersect(const Vector & u0, const Vector & u1, const Vector & u2,
                  const Vector & v0, const Vector & v1, const Vector & v2)
{
    Plane pi2(v0,v1,v2);

    float du0 = pi2.evalPoint(u0),
        du1 = pi2.evalPoint(u1),
        du2 = pi2.evalPoint(u2);

    int s0 = signZ(du0);
    int s1 = signZ(du1);
    int s2 = signZ(du2);
    
    bool different_sides_1 = s1 != s0;
    bool different_sides_2 = s2 != s0;

    // if all points are classified the same, triangles are either coplanar or
    // separated.
    if (!different_sides_1 && !different_sides_2)
    {
        return false;
        return s0 == 0;
    }
    
    if (!(different_sides_1 && different_sides_2 && s1 != s2) &&
        !(s0 != 0 && s1 != 0 && s2 != 0)) return false;

    Vector p, q;
    if (different_sides_1)
    {
        p = u0 + du0 / (du0 - du1) * (u1 - u0);
        if (different_sides_2)
        {
            q = u0 + du0 / (du0 - du2) * (u2 - u0);
        } else
        {
            q = u2 + du2 / (du2 - du1) * (u1 - u2);
        }
    } else
    {
        p = u2 + du2 / (du2 - du0) * (u0 - u2);
        q = u2 + du2 / (du2 - du1) * (u1 - u2);
    }

    unsigned axis_max_area = abs(pi2.normal_.x_) > abs(pi2.normal_.y_) ?
        (abs(pi2.normal_.x_) > abs(pi2.normal_.z_) ? 0 : 2) :
        (abs(pi2.normal_.y_) > abs(pi2.normal_.z_) ? 1 : 2);
    
    if (axis_max_area == 0)
    {
        return triOverlapsSegment(Vector2d(v0.y_, v0.z_),
                                  Vector2d(v1.y_, v1.z_),
                                  Vector2d(v2.y_, v2.z_),
                                  Vector2d(p.y_, p.z_),
                                  Vector2d(q.y_, q.z_));
    } else if (axis_max_area == 1)
    {
        return triOverlapsSegment(Vector2d(v0.x_, v0.z_),
                                  Vector2d(v1.x_, v1.z_),
                                  Vector2d(v2.x_, v2.z_),
                                  Vector2d(p.x_, p.z_),
                                  Vector2d(q.x_, q.z_));
    } else
    {
        return triOverlapsSegment(Vector2d(v0.x_, v0.y_),
                                  Vector2d(v1.x_, v1.y_),
                                  Vector2d(v2.x_, v2.y_),
                                  Vector2d(p.x_, p.y_),
                                  Vector2d(q.x_, q.y_));
    }
}

//------------------------------------------------------------------------------
/**
 *  Clips the polygon to the specified window using the
 *  cohen-sutherland algorithm.
 */
std::vector<Vector2d> clipPoly(std::vector<Vector2d> p,
                               float minx, float maxx,
                               float miny, float maxy)
{
    std::vector<Vector2d> ret;

    // Traverse window edges and clip lines
    for (unsigned num_edge=0; num_edge<4; ++num_edge)
    {
        Line2d edge;
        switch(num_edge)
        {
        case 0:
            edge = Line2d(Vector2d(-1, 0), Vector2d(minx, 0));
            break;
        case 1:
            edge = Line2d(Vector2d(1, 0), Vector2d(maxx, 0));
            break;
        case 2:
            edge = Line2d(Vector2d(0, -1), Vector2d(0, miny));
            break;
        case 3:
            edge = Line2d(Vector2d(0, 1), Vector2d(0, maxy));
            break;
        }

        
        for (unsigned p1=0; p1<p.size(); ++p1)
        {
            unsigned p2 = p1==p.size()-1 ? 0:p1+1;

            bool in1 = edge.evalPoint(p[p1]) < 0;
            bool in2 = edge.evalPoint(p[p2]) < 0;

            // Drop edge if both points are outside
            if (!in1 && !in2) continue;

            // If both points are inside, add p1 to result and
            // continue
            if (in1 && in2)
            {
                ret.push_back(p[p1]);
                continue;
            }

            // Now we know there is an intersection. First add p1 if
            // its inside, then add intersection point.
            if (in1) ret.push_back(p[p1]);

            ret.push_back(edge.clip(p[p1], p[p2]));
        }

        if (num_edge != 3)
        {
            p=ret;
            ret.clear();
        }
    }

    return ret;    
}



//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const AABB & aabb)
{
    out << aabb.min_ << " " << aabb.max_;

    return out;
}

//------------------------------------------------------------------------------
std::istream & operator>>(std::istream & in, AABB & )
{
    assert(!"not yet implemented");

    return in;
}

namespace serializer
{

//------------------------------------------------------------------------------    
void putInto(Serializer & s, const AABB & aabb)
{
    s.put(aabb.min_);
    s.put(aabb.max_);
}

//------------------------------------------------------------------------------    
void putInto(Serializer & s, const BoundingSphere & bs)
{
    s.put(bs.center_);
    s.put(bs.radius_);
}

//------------------------------------------------------------------------------
void getFrom(Serializer & s, AABB & aabb)
{
    s.get(aabb.min_);
    s.get(aabb.max_);
}

//------------------------------------------------------------------------------
void getFrom(Serializer & s, BoundingSphere & bs)
{
    s.get(bs.center_);
    s.get(bs.radius_);
}

} // namespace serializer
