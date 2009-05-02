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
 *  filename            : Utils.cpp
 *  author              : Muschick Christian
 *  date of creation    : 13.01.2004
 *  date of last change : 13.01.2004
 *
 *******************************************************************************/

#include "Utils.h"


#include <fstream>


#ifndef _WIN32

#include <unistd.h>
#include <fcntl.h>

#ifdef _DEBUG
#include <fenv.h>
#endif

#endif



#ifdef _WIN32
#include <float.h> // used for floating point execptions
#endif


#include "utility_Math.h"
#include "Log.h"

//------------------------------------------------------------------------------
/**
 *  Adds a carriage return after every linebreak character found in
 *  the input string. Useful for dedicated server raw terminal mode.
 */
std::string addCr(const std::string & input)
{
    std::string ret = input;
    std::string::size_type cur_pos = 0;
    while ((cur_pos = ret.find('\n', cur_pos)) != std::string::npos)
    {
        ret.replace(cur_pos, 1, "\n\r");
        cur_pos += 2;
        if (cur_pos==ret.size()) break;
    }
    return ret;
}



//------------------------------------------------------------------------------
/**
 *  Reads the contents of the specified text file into a string.
 */
std::string readTextFile(const std::string &filename)
{
    std::ifstream in(filename.c_str());
    if (!in)
    {
        Exception e("Text file ");
        e << filename << " could not be opened for reading.";
        throw e;
    }

    std::string ret,line;
    
    while (std::getline(in, line))
    {
        ret += line;
        ret += "\n";
    }

    return ret;
}


//------------------------------------------------------------------------------
/**
 *  Deletes leading & trailing whitespaces of a string.
 */
void trim(std::string & str)
{
    if (str.empty()) return;
    
    int head = 0;
    int tail = (int)str.length()-1;
    
    while(head <  tail && isspace(str[head])) head++;
    while(tail >= 0    && isspace(str[tail])) tail--;
    
    if (tail < head) str = "";
    else str = str.substr(head, tail-head+1);
}


//------------------------------------------------------------------------------
/**
 *  Returns the number of characters ch in the string.
 */
unsigned charCount(const std::string & str, char ch)
{
    unsigned ret=0;
    for (const char* cur_char = str.c_str(); *cur_char != '\0'; ++cur_char)
    {
        if (*cur_char == ch) ++ret;
    }

    return ret;
}

//------------------------------------------------------------------------------
/**
 *  Subdivide & displace algorithm.
 *  First sets the corner values to half the maximum height, then iteratively
 *  calculates the interpolated height values of center and edge vertices
 *  and adds a random offset attenuated by a cos-function and amplified
 *  by a power function dependend on roughness.
 *
 *  \param data      A pointer to a size*size float array.
 *  \param size      Must be 2^n +1.
 *  \param roughness Can be pos. or neg.
 *  \param maxHeight Determines max amplitude.
 */
void generateSnD(float *data, unsigned size, float roughness, float maxHeight)
{

  if (size < 5)
  {
        Exception e;
        e << "GenerateSnD() : size must be 5 at least but is " << size << ".";
        throw e;
  }
    if (!isPowerOfTwo(size-1))
    {
        Exception e;
        e << "GenerateSnD() : size must be 2^n +1 but is " << size << ".";
        throw e;
    }
    
    // set corner values
    data[0]             = maxHeight/2.0f;
    data[size-1]        = maxHeight/2.0f;
    data[(size-1)*size] = maxHeight/2.0f;
    data[size*size-1]   = maxHeight/2.0f;


    float range;        // range of random offsets
    float value;        // interpolated height value
    float offset;       // random offset to value
    int maxIteration = ld(size-1)-1;

    // squareSize = size of new square
    for (unsigned squareSize = (size-1)>>1, iteration=0; squareSize >= 1; squareSize >>=1, iteration++)
    {
        range = maxHeight *(cosf(iteration/maxIteration *PI)+1.0f);
        range *= pow(iteration+1.0f, roughness);


        // calculate height at square center
        for (unsigned row=squareSize; row <= size-1-squareSize; row+=squareSize<<1)
        {
            for (unsigned col=squareSize; col <= size-1-squareSize; col+=squareSize<<1)
            {
                value = (data[row-squareSize + (col-squareSize)*size] +
                         data[row+squareSize + (col-squareSize)*size] +
                         data[row-squareSize + (col+squareSize)*size] +
                         data[row+squareSize + (col+squareSize)*size]) / 4.0f;


                offset = ((float)rand()/RAND_MAX-0.5f)*range;
                if (value + offset > maxHeight) offset = maxHeight-value;
                else if (value + offset < 0.0f) offset = -value;

                value += offset;

                data[row + col*size] = value;
            }
        }

        // calc height at edges
        for (unsigned col = 0; col < size; col+=squareSize)
        {
            for(unsigned row = col%(2*squareSize)==0?squareSize:0; row < size; row += squareSize<<1)
            {
                int c=0;
                value=0;
                if (row != 0)
                {
                    c++;
                    value += data[row-squareSize + (col)           *size];
                }
                if (col != 0)
                {
                    c++;
                    value += data[row            + (col-squareSize)*size];
                }
                if (row != size-1)
                {
                    c++;
                    value += data[row+squareSize + (col)           *size];
                }
                if (col != size-1)
                {
                    c++;
                    value += data[row            + (col+squareSize)*size];
                }
                value /= c;

                offset = ((float)rand()/RAND_MAX-0.5f)*range;
                if (value + offset > maxHeight) offset = maxHeight-value;
                else if (value + offset < 0) offset = -value;

                value += offset;

                data[row + (col)*size] = value;

            }
        }
    }
}


//------------------------------------------------------------------------------
void enableFloatingPointExceptions(bool v)
{
#ifdef _DEBUG
#ifdef _WIN32
    // always call _clearfp before masking a FPU exception    
    _clearfp();
    if(v)
    {
        _controlfp_s(NULL, _EM_INEXACT | _EM_UNDERFLOW , _MCW_EM);
    }
    else
    {
        _controlfp_s(NULL, _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID | _EM_DENORMAL | _EM_INEXACT | _EM_UNDERFLOW , _MCW_EM);
    }
#else

    feclearexcept( FE_ALL_EXCEPT );
    if(v)
    {
        feenableexcept( FE_DIVBYZERO | FE_OVERFLOW /*| FE_INVALID */ );
    } else
    {
        fedisableexcept( FE_ALL_EXCEPT );
    }
#endif
#endif
}


//------------------------------------------------------------------------------
/**
 *  Determines whether a file exists by opening it.
 */
bool existsFile(const char * filename)
{
#ifdef _WIN32
    FILE * f = fopen(filename, "r");
    if (f) fclose(f);
#else
    int f = open(filename, O_RDONLY);
    if (f) close(f);
#endif
    return f > 0;
}

