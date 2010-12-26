
#ifndef LIB_TIME_INCLUDED
#define LIB_TIME_INCLUDED

#ifdef _WIN32

#include "Datatypes.h"

typedef DWORD TimeValue;

#else

#include <sys/time.h>
#include <unistd.h>

typedef timeval TimeValue;

#endif


float getMillis(const TimeValue & time);
float getTimeDiff(const TimeValue & t2, const TimeValue & t1);
void getCurTime(TimeValue & time);    
void sleepMs(unsigned msecs);


#endif // #ifndef LIB_TIME_INCLUDED
