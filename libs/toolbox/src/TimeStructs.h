
#ifndef LIB_TIME_INCLUDED
#define LIB_TIME_INCLUDED

#ifdef _WIN32

#include "Datatypes.h"

typedef DWORD TimeValue;

#else

#include <sys/time.h>

typedef timeval TimeValue;

#endif


float getMillis(const TimeValue & time);
float getTimeDiff(const TimeValue & t2, const TimeValue & t1);
void getCurTime(TimeValue & time);    



#endif // #ifndef LIB_TIME_INCLUDED
