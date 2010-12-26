
#include "TimeStructs.h"

//------------------------------------------------------------------------------
/**
 *  Converts the specified time to milliseconds.
 */
float getMillis(const TimeValue & time)
{
#if _WIN32
    return (float)time;
#else
    return time.tv_sec * 1000.0f + time.tv_usec / 1000.0f;
#endif 
}


//------------------------------------------------------------------------------
/**
 *  Returns the difference between the two times in milliseconds.
 */
float getTimeDiff(const TimeValue & t2, const TimeValue & t1)
{
#if _WIN32
    return (float)((int)t2-(int)t1);
#else
    return ((int)t2.tv_sec - (int)t1.tv_sec) * 1000.0f +
        ((int)t2.tv_usec - (int)t1.tv_usec) / 1000.0f;
#endif 
}


//------------------------------------------------------------------------------
/**
 *  Gets the current time.
 */
void getCurTime(TimeValue & time)
{
#if _WIN32
    time = timeGetTime();
#else
    gettimeofday(&time, 0);
#endif
}

//------------------------------------------------------------------------------
void sleepMs(unsigned msecs)
{
#ifdef _WIN32
    ::Sleep((DWORD)msecs);
#else
    usleep(1000*msecs);
#endif
}
