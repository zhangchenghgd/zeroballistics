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
 *  filename            : Time.h
 *  author              : Muschick Christian
 *  date of creation    : 03.09.2004
 *  date of last change : 03.09.2004
 *
 *
 *******************************************************************************/

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
