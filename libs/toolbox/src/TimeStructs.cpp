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
 *  filename            : Time.cpp
 *  author              : Muschick Christian
 *  date of creation    : 03.09.2004
 *  date of last change : 03.09.2004
 *
 *******************************************************************************/

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
    return (float)(t2-t1);
#else
    return (t2.tv_sec - t1.tv_sec) * 1000.0f +
        (t2.tv_usec - t1.tv_usec) / 1000.0f;
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

