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
 *  filename            : Geometry.h
 *  author              : Muschick Christian
 *  date of creation    : 22.08.2003
 *  date of last change : 03.01.2004
 *
 *
 *******************************************************************************/

#ifndef STUNTS_GEOMETRY_INCLUDED
#define STUNTS_GEOMETRY_INCLUDED


#include "Vector.h"
#include "Plane.h"

//------------------------------------------------------------------------------
/**
 *  An axis-aligned bounding box.
 */
struct AABB
{
    AABB();
    AABB(const Vector & min, const Vector & max);
    
    AABB & operator=(const Vector & p);
    void enlarge(const Vector & p);
    void merge(const AABB & aabb);

    Vector getSize() const;
    Vector getCenter() const;
    
    Vector min_;
    Vector max_;
};

//------------------------------------------------------------------------------
struct BoundingSphere
{
    Vector center_;
    float32_t radius_;
};

//------------------------------------------------------------------------------
class Box2d
{
 public:
    Box2d();
    Box2d(Vector2d top_left, Vector2d bottom_right);
    Box2d(Vector2d v);
    ~Box2d();

    Box2d & operator=(Vector2d v);
    
    bool isInside(Vector2d v);
    void extendBy(Vector2d v);

    Vector2d top_left_;
    Vector2d bottom_right_;
};


//------------------------------------------------------------------------------
class Line2d
{
 public:
    Line2d();
    Line2d(Vector2d n, Vector2d p);

    float evalPoint(Vector2d v) const;
    Vector2d clip(Vector2d p1, Vector2d p2) const;
    
    Vector2d n_;
    float c_;
};

//------------------------------------------------------------------------------
/**
 *  Represents a segment (x = p1 + s*(p2 - p1), s e [0, 1]).
 */
class Segment
{
public:
    Segment();
    Segment(const Vector & p1, const Vector & p2);

    void set(const Vector & p1, const Vector & p2);
    void setDir(const Vector & p, const Vector & dir);

    Vector p_;     ///< The "tail" of the segment.
    Vector d_;     ///< The direction of the segment (p2 - p1).
};

float segCosAngle(const Segment * seg1, const Segment * seg2);
void segGetClosestPoints(Vector * x0, Vector * x1, const Segment * seg1, const Segment * seg2);
void segGetClosestPoint(Vector * x, const Vector * p, const Segment * seg);
void segGetClosestPoint(Vector * x, const Plane * plane, const Segment * seg);
float segDistanceRay(const Segment * seg, const Vector * v);


bool triIntersect(const Vector & u0, const Vector & u1, const Vector & u2,
                  const Vector & v0, const Vector & v1, const Vector & v2);


std::vector<Vector2d> clipPoly(const std::vector<Vector2d> p,
                               float minx, float maxx,
                               float miny, float maxy);

std::ostream & operator<<(std::ostream & out, const AABB & aabb);
std::istream & operator>>(std::istream & in, AABB & aabb);


namespace serializer
{
    
void putInto(Serializer & s, const AABB & aabb);
void putInto(Serializer & s, const BoundingSphere & bs);

void getFrom(Serializer & s, AABB & aabb);
void getFrom(Serializer & s, BoundingSphere & bs);
 
}

#endif // #ifndef STUNTS_GEOMETRY_INCLUDED
