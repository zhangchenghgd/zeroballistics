/**

 *  This file should add Ogg Vorbis support for OpenAL
 *  The function alutLoadOgg() is the equivalent to alutLoadWAV()
 **/

#ifndef _OGGVORBIS_H_
#define _OGGVORBIS_H_

#include <cmath>
#include <string>


#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

#include "AL/al.h"

ALboolean loadOgg(  const char *fname,
                        void **wave,
			ALsizei *format,
			ALsizei *size,
			ALsizei *bits,
			ALsizei *freq );


void unloadOgg(ALenum format,
		   ALvoid *data,
		   ALsizei size,
		   ALsizei freq);


#endif
