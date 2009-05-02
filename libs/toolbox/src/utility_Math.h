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
 *  filename            : utility_Math.h
 *  author              : Muschick Christian
 *  date of creation    : 13.01.2004
 *  date of last change : 16.07.2004
 *
 *  Various math functions and constants.
 *
 *******************************************************************************/

#ifndef STUNTS_UTIL_MATH_INCLUDED
#define STUNTS_UTIL_MATH_INCLUDED


#include <cmath>

#include <loki/Functor.h>

#include "Datatypes.h"

#ifdef _WIN32
#define copysign _copysign
#endif

// sadly, fox defines this as a preprocessor constant
#undef PI


typedef Loki::Functor<float, LOKI_TYPELIST_1(float)> FloatFun;
typedef Loki::Functor<void, LOKI_TYPELIST_3(float, float&, float&)> NewtonFun;



const float E = 2.71828182845905f;
const float PI = 3.14159265359f;
const float EPSILON = 1.0e-4f;



float simpson(float t0, float t1, FloatFun fun, unsigned num_intervals);

float newton(float t0, NewtonFun fun, float max_change, float max_count = 1500);

float findExtremum(float t0, FloatFun fun, bool max, float initial_step = 1.0f,
                   float min_step = EPSILON, unsigned max_iters = 500);


unsigned solveQuadratic(float a, float b, float c, float & r1, float & r2);

std::vector<unsigned> randomSequence(unsigned start, unsigned size);

uint32_t createTrueRandom();
float randNormal(float avg, float std_dev);

float deg2Rad(float deg);
float rad2Deg(float rad);
float normalizeAngle(float a);

int sign(int i);
int sign(float f);
int signZ(float f);

bool equalsZero(float f, float epsilon = EPSILON);

float maxAbsf(float f1, float f2);
float minAbsf(float f1, float f2);
float maxf(float f1, float f2);
float minf(float f1, float f2);
float maxf(float f1, float f2, float f3, float f4);
float minf(float f1, float f2, float f3, float f4);

inline float round(float v) { return v>=0.0f?floorf(v+0.5f):ceilf(v-0.5f); }
inline double round(double v) { return v>=0.0?floor(v+0.5):ceil(v-0.5); }

bool isPowerOfTwo(unsigned i);
unsigned ld(unsigned i);

//------------------------------------------------------------------------------
template<class T>
T clamp(T a, T l, T u)
{
    return a<l?l:
        (a>u?u:a);
}

//------------------------------------------------------------------------------
template<class T>
T sqr(T t)
{
    return t*t;
}

//------------------------------------------------------------------------------
template<class T>
T abs(T t)
{
    return t > 0 ? t:-t;
}


#endif // #ifndef STUNTS_UTIL_MATH_INCLUDED
