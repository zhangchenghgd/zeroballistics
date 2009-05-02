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
 *  filename            : Datatypes.h
 *  author              : Muschick Christian
 *  date of creation    : 14.04.2003
 *  date of last change : 01.12.2004
 *
 *
 *******************************************************************************/

#ifndef STUNTS_DATATYPES_INCLUDED
#define STUNTS_DATATYPES_INCLUDED

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

typedef bool          bool8_t;
typedef unsigned char uint8_t;
typedef WORD          uint16_t;
typedef short int     int16_t;
typedef unsigned int  uint32_t;
typedef int           int32_t;
typedef UINT64        uint64_t;
typedef INT64         int64_t;
typedef float         float32_t;

#else
#include <stdint.h>

typedef bool bool8_t;
typedef float float32_t;

#endif

#include <iostream>
#include <vector>
#include <iomanip>



const float COLOR_U2F_FACTOR = 0.00392156862745098f;  // 1 / 255
inline float COLOR_U2F(unsigned c) { return (float)c*COLOR_U2F_FACTOR; }
inline unsigned COLOR_F2U(float c)    { return (unsigned)(c*255.0f); }

//------------------------------------------------------------------------------
struct Color
{
    Color() {}
    Color(unsigned r, unsigned g, unsigned b) : r_(COLOR_U2F(r)), g_(COLOR_U2F(g)), b_(COLOR_U2F(b)) {}
    Color(float r, float g, float b) : r_(r), g_(g), b_(b), a_(1.0f) {}
    Color(float r, float g, float b, float a) : r_(r), g_(g), b_(b), a_(a) {}


    Color & operator+=(const Color & c);
    Color & operator*=(float f);

    Color operator*(float f) const;

    float getBrightness() const;
    
    float r_;
    float g_;
    float b_;
    float a_;
};



//------------------------------------------------------------------------------
struct TexCoord
{
    TexCoord() {}
    TexCoord(float tu, float tv) : tu_(tu), tv_(tv) {}
    float tu_;
    float tv_;
};


//------------------------------------------------------------------------------
struct Material
{
    Material() :
    ambient_(Color(1.0f,0.0f,1.0f)), diffuse_(Color(1.0f,0.0f,1.0f)),
    specular_(Color(1.0f,0.0f,1.0f)), shininess_(0) {}
    
    Color ambient_;
    Color diffuse_;
    Color specular_;
    uint32_t shininess_;
};


std::istream & operator>>(std::istream & in, Color & color);
std::ostream & operator<<(std::ostream & out, const Color & color);
std::istream & operator>>(std::istream & in, TexCoord & t);
std::ostream & operator<<(std::ostream & out, const TexCoord & t);


namespace serializer
{
    class Serializer;

    void putInto(Serializer & s, const Color & c);    
    void getFrom(Serializer & s, Color & c);

    void putInto(Serializer & s, const Material & m);    
    void getFrom(Serializer & s, Material & m);

    void putInto(Serializer & s, const TexCoord & t);
    void getFrom(Serializer & s, TexCoord & t);
    
}



#endif // #ifndef STUNTS_DATATYPES_INCLUDED
