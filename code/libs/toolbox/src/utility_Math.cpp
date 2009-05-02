

#include "utility_Math.h"


#include <iostream>
#include <fcntl.h>


#include "Exception.h"


//------------------------------------------------------------------------------
/**
 *  Implements simpsons formula to calculate the integral from t0 to
 *  t1 of fun.
 *
 *  \param t0 The lower limit of integration.
 *  \param t1 The upper limit of integration.
 *  \param fun The function to integrate.
 *  \param num_intervals The number of 'double intervals'. Must be >= 1.
 */
float simpson(float t0, float t1, FloatFun fun, unsigned num_intervals)
{
    assert(num_intervals != 0);
    
    float dt = (t1-t0) / (2*num_intervals);

    float sum = (fun(t0) + fun(t1));

    float cur_t = t0 + dt;
    for (unsigned i=0; i < num_intervals-1; ++i)
    {
        sum += 4.0f*fun(cur_t);
        cur_t += dt;
        sum += 2.0f*fun(cur_t);
        cur_t += dt;
    }
    sum += 4.0f*fun(cur_t);

    return sum*(t1 - t0) / (6*num_intervals);
}


//------------------------------------------------------------------------------
/**
 *  Implements newton's method to find a root of a differentiable
 *  function.
 *
 *  \param t0 The initial guess for the newton iteration.
 *  \param fun The function calculating the function value and first derivative.
 *  \param max_change If the found value changes less than this value
 *                    in an iteration, the function returns.
 *  \param max_count Maximum number of iteration. If this value is reached, the function bails.
 *
 *  \return A guess for a root of fun.
 */
float newton(float t0, NewtonFun fun, float max_change, float max_count)
{
    float t = t0;
    unsigned count = 0;
    float d1, d2, dt;
    do
    {
        fun(t, d1, d2);
                
        dt = d1 / d2;
        t -= dt;

        count++;
        if (count > max_count)
        {
            std::cerr << "Newton doesn't converge\n";
            return t;
        }
    } while (dt > max_change);    

    return t;
}

//------------------------------------------------------------------------------
/**
 *  Tries to find an extremum of a function by gradient descent.
 *
 *  Very crude approach, don't expect this to be very exact!
 *
 *  \param t0 The initial guess for the extremum.
 *  \param fun The function for which to find the extremum.
 *  \param max True if searching for the maximum, false if searching the minimum.
 *  \param initial_step The initial step size.
 *  \param min_step Bails if the update step becomes smaller than this value.
 *  \param max_iters Bails if more than max_iters iterations have been calculated.
 *
 *  \return A guess for an extremum of fun.
 */
float findExtremum(float t0, FloatFun fun, bool max, float initial_step,
                   float min_step, unsigned max_iters)
{
    unsigned num_iters = 0;
    float derivative, last_derivative = 0.0f;
    float step_size    = initial_step;
    float cur_t        = t0;
    
    do
    {
        if (max)
        {
            derivative = (fun(cur_t + EPSILON) - fun(cur_t));
        } else
        {
            derivative = (fun(cur_t) - fun(cur_t + EPSILON));
        }

        // reduce step size on derivative sign reversal
        if ((derivative > 0.0f && last_derivative < 0.0f) ||
            (derivative < 0.0f && last_derivative > 0.0f))
        {
            step_size *= 0.9f;
        }

        cur_t += sign(derivative)*step_size;

        last_derivative = derivative;
        ++num_iters;
    } while(step_size > min_step && num_iters < max_iters);

    return cur_t;
}


//------------------------------------------------------------------------------
/**
 *  Solves an equation of the form ax^2+bx+c=0.
 *
 *  \param a The coefficient for x^2.
 *  \param b The coefficient for x^1.
 *  \param c The coefficient for x^0.
 *  \param r1 [out] Is set to the first real root if existent.
 *  \param r2 [out] Is set to the second real root if existent.
 *  \return The number of real roots found.
 */
unsigned solveQuadratic(float a, float b, float c, float & r1, float & r2)
{
    float discriminant = b*b - 4.0f*a*c;

    if (equalsZero(discriminant))
    {
        r1 = -b/a*0.5f;
        return 1;
    } else if ( discriminant > 0.0f)
    {
        r1 = (-b + discriminant)/a*0.5f;
        r2 = (-b - discriminant)/a*0.5f;
        return 2;
    } else return 0;
}

//------------------------------------------------------------------------------
/**
 *  Returns a random sequence containing all numbers from start to
 *  start+size-1.
 */
std::vector<unsigned> randomSequence(unsigned start, unsigned size)
{
    std::vector<unsigned> ret(size);

    for (unsigned n=0; n<size; ++n)
    {
        ret[n] = start+n;
    }
    
    for (unsigned n=0; n<size; ++n)
    {
        unsigned r = (unsigned)floor((float)rand()/RAND_MAX * (size-1));
        assert (r < size);
        
        std::swap(ret[n], ret[r]);
    }

    return ret;
}



//------------------------------------------------------------------------------
/**
 *  Uses dev/urandom to create a truly random authentication token.
 *
 *  see CryptGenRandom, CryptAcquireContext for win implementation
 */
uint32_t createTrueRandom()
{
    uint32_t ret;
#ifdef _WIN32
    ret = rand(); ///< XXX hacky
#else
    int urandom = open("/dev/urandom", O_RDONLY);

    if (urandom < 0) throw Exception("Unable to open urandom");
    
    if (read(urandom, &ret, sizeof(uint32_t)) != sizeof(uint32_t))
    {
        throw Exception("Couldn't read from urandom");
    }
    
    if (close(urandom) < 0) throw Exception("Unable to close urandom");
#endif
    return ret;
}


//------------------------------------------------------------------------------
/**
 *  Returns normally distributed random variables.
 */
float randNormal(float avg, float std_dev)
{
    float r1 = (float)rand()/RAND_MAX;
    float r2 = (float)rand()/RAND_MAX;
    
    float ret = sqrtf(-2.0f*log(1-r1))*cosf(r2*2.0f*PI);

    ret *= std_dev;
    ret += avg;

    return ret;
}



//------------------------------------------------------------------------------
float deg2Rad(float deg)
{
    static const float DEG2RAD_FACTOR = PI/180.0f;
    return deg*DEG2RAD_FACTOR;
}

//------------------------------------------------------------------------------
float rad2Deg(float rad)
{
    static const float RAD2DEG_FACTOR = 180.0f/PI;
    return rad*RAD2DEG_FACTOR;
}

//------------------------------------------------------------------------------
float normalizeAngle(float a)
{
    while (a <  0.0f) a += 2*PI;
    while (a >= 2*PI) a -= 2*PI;

    return a;
}



//------------------------------------------------------------------------------
/**
 *  \returns 1 if f is greater than zero, -1 if it's smaller than zero,
 */
int sign(float f)
{
    return f > 0.0f ? 1 : -1;
}

//------------------------------------------------------------------------------
/**
 *  \returns 1 if f is greater than epsilon, -1 if it's smaller than -epsilon,
 *  0 otherwise.
 */
int signZ(float f)
{
    return f > EPSILON ? 1 : f < -EPSILON ? -1 : 0;
}

//------------------------------------------------------------------------------
bool equalsZero(float f, float epsilon)
{
    return f < epsilon && f > -epsilon;
}


//------------------------------------------------------------------------------
int sign(int i)
{
    return i > 0 ? 1 : i < 0 ? -1 : 0;
}


//------------------------------------------------------------------------------
float maxAbsf(float f1, float f2)
{
    return abs(f1) > abs(f2) ? f1:f2;
}


//------------------------------------------------------------------------------
float minAbsf(float f1, float f2)
{
    return abs(f1) < abs(f2) ? f1:f2;
}



//------------------------------------------------------------------------------
float maxf(float f1, float f2)
{
    return f1>f2 ? f1:f2;
}

//------------------------------------------------------------------------------
float minf(float f1, float f2)
{
    return f1<f2 ? f1:f2;
}

//------------------------------------------------------------------------------
float maxf(float f1, float f2, float f3, float f4)
{
    return maxf(maxf(f1, f2), maxf(f3, f4));
}

//------------------------------------------------------------------------------
float minf(float f1, float f2, float f3, float f4)
{
    return minf(minf(f1, f2), minf(f3, f4));
}



//------------------------------------------------------------------------------
/**
 *  Checks if the argument is a power of two. This is done by
 *  checking whether there is only one '1' in the binary representation and it is
 *  not the LSB.
 *  Shift until first '1' is shifted out, then return whether there is
 *  another one.
 */
bool isPowerOfTwo(unsigned i)
{
    if (i & 1) return i == 1;
    if (i == 0) return false;
    while (!((i >>= 1) & 1));
    return !(i >> 1);
}

//------------------------------------------------------------------------------
/**
 *  Slow logarithmus dualis. Counts the rightshifts that must be performed before
 *  the number is zero.
 */
unsigned ld(unsigned i)
{
    if (i==0) return (unsigned) -1;
    if (i==1) return 0;

    unsigned res = 0;
    while (i >>= 1) res++;
    return res;
}


