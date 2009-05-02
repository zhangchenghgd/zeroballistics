




#ifndef LIB_UTILS_INCLUDED
#define LIB_UTILS_INCLUDED

#include <string>
#include <iostream>
#include <cmath>
#include <sstream>


#include "Datatypes.h"


#include "Exception.h"
#include "utility_Math.h"


#define UNUSED_VARIABLE(v) (void)v;
#define DELNULL(p)      if(p) { delete    p; p=NULL; }
#define DELNULLARRAY(p) if(p) { delete [] p; p=NULL; }


std::string addCr(const std::string & input);
std::string readTextFile(const std::string &filename);
void trim(std::string & str);
unsigned charCount(const std::string & str, char ch);
bool existsFile(const char * filename);
void generateSnD(float *data, unsigned size, float roughness, float maxHeight);
void enableFloatingPointExceptions(bool v = true);



//------------------------------------------------------------------------------
/**
 *  Smoothes a square array by averaging with the 8 surrounding elements.
 *
 *  \param data       The data to smooth. must be width*height elements.
 *  \param width      The width of the array.
 *  \param height     The height of the array.
 *  \param iterations The number of times to apply the smoothing filter.
 */
template <class T>
void averageArray(T *data, unsigned width, unsigned height, unsigned iterations)
{    
    double value;

    T * new_data = new T[width*height];
    T * tmp_pointer;
    
    for (unsigned it=0; it<iterations; ++it)
    {
        for (unsigned row=0; row<height; ++row)
        {
            for (unsigned col=0; col<width; ++col)
            {
                value = data[col + width*row];
                int c = 1;

                if (col != 0)
                {
                    c++;
                    value += data[col-1 + width* row   ];
                }
                if (col != width-1)
                {
                    c++;
                    value += data[col+1 + width* row   ];
                }
                if (row != 0)
                {
                    c++;
                    value += data[col   + width*(row-1)];
                }
                if (row != height-1)
                {
                    c++;
                    value += data[col   + width*(row+1)];
                }
			
                value /= c;
                new_data[col + width*row] = (T)value;
            }
        }
        tmp_pointer = new_data;
        new_data    = data;
        data        = tmp_pointer;
    }

    if (iterations & 1)
    {
        memcpy(new_data, data, sizeof(T)*width*height);
        delete [] data;            
    } else
    {
        delete [] new_data;
    }

}


//------------------------------------------------------------------------------
/**
 *  Flips a data array vertically.
 *  This is done by exchanging lines (topmost with bottommost, then
 *  converging to the center).
 *
 *  \param width  The width of the data array.
 *  \param height The height of the data array.
 *  \param data   The data to be flipped.
 */
template <class T>
void flipArrayVert(unsigned width, unsigned height, T * data)
{
    T * tmp_line = new T[width];

    for (unsigned row=0; row < height/2; ++row)
    {
        memcpy(tmp_line,                    &data[row*width],            sizeof(T)*width);
        memcpy(&data[row*width],            &data[(height-1-row)*width], sizeof(T)*width);
        memcpy(&data[(height-1-row)*width], tmp_line,                    sizeof(T)*width);
    }
    
    delete [] tmp_line;
}


//------------------------------------------------------------------------------
/**
 *  A template class representing a Node from a Quadtree.
 */
template<class T> class QuadNode
{
 public:
    
//------------------------------------------------------------------------------
/**
 *  Build a quadtree with the specified depth.
 *  depth is number of levels-1.
 */
    QuadNode(unsigned depth, T * parent = NULL) : parent_(parent)
        {
            if (depth)
            {
                upper_left_  = new T(depth-1, (T*)this);
                upper_right_ = new T(depth-1, (T*)this);
                lower_left_  = new T(depth-1, (T*)this);
                lower_right_ = new T(depth-1, (T*)this);
            } else
            {
                upper_left_  = NULL;
                upper_right_ = NULL;
                lower_left_  = NULL;
                lower_right_ = NULL;
            }
        }
    
//------------------------------------------------------------------------------
    virtual ~QuadNode()
        {
            DELNULL(upper_left_);
            DELNULL(upper_right_);
            DELNULL(lower_left_);
            DELNULL(lower_right_);
        }

//------------------------------------------------------------------------------
    bool isLeaf() const
        {
            return upper_left_ == NULL;
        }
    
//------------------------------------------------------------------------------
/**
 *  Expensive recursive function to determine the tree's depth.
 */
    unsigned getDepth() const
        {
            if (upper_left_) return upper_left_->getDepth() +1;
            else return 0;
        }

    T * parent_;
    
    T * upper_left_;
    T * upper_right_;
    T * lower_left_;
    T * lower_right_;

 protected:
    QuadNode() {}
};

//------------------------------------------------------------------------------
/**
 *  toString conversion with any argument type
 */
template <typename T>
inline std::string toString(T t)
{
    std::ostringstream s;
    s << t;
    return s.str();
}

//------------------------------------------------------------------------------
/**
 *  toString specialization for uint8_t
 */
template < >
inline std::string toString(uint8_t t)
{
    std::ostringstream s;
    s << (unsigned)t;
    return s.str();
}

//------------------------------------------------------------------------------
/**
 *  fromString conversion with any argument type
 *  i.e. int x = fromString<int>(text_string);
 */
template<typename RT, typename T, typename Trait, typename Alloc>
inline RT fromString( const std::basic_string<T, Trait, Alloc>& the_string )
{
    std::basic_istringstream< T, Trait, Alloc> temp_ss(the_string);
    RT num;
    temp_ss >> num;
    return num;
}

template< >
inline std::string fromString<std::string>( const std::string & the_string )
{
    std::istringstream temp_ss(the_string);
    std::string sub_str;
    std::string value;
    while (temp_ss >> sub_str)
    {
        if (!value.empty()) value += ' ';
        value += sub_str;
    }

    return value;
}

#endif // ifndef LIB_UTILS_INCLUDED
