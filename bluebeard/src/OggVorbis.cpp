

#include "OggVorbis.h"

#include <string.h>

#include "Exception.h"


ALboolean loadOgg(  const char *fname,
                    void **wave,
                    ALsizei *format,
                    ALsizei *size,
                    ALsizei *bits,
                    ALsizei *freq )
{
    *format  = 0;

    OggVorbis_File vf;
    memset(&vf, 0, sizeof(vf));
    int eof = 0;
    int current_section;

  
    long ret;

    FILE * file = fopen(fname,"rb+");


//ov_open could take long and should be called in a thread
    //INFO: If the program crashes here and you are compiling under windows, make sure your
    //      debugging option equals "Multithreaded DLL"
    if(ov_open(file, &vf, NULL, 0) < 0) {
        fprintf(stderr,"\n");
        fclose(file);
        throw Exception(std::string("Input ") + fname + " does not appear to be an Ogg bitstream.");
    }

    {
        vorbis_info *vi=ov_info(&vf,-1);

        *freq = (ALsizei) vi->rate;
        /************* HACK? ************/
        *size = (ALsizei) ov_pcm_total(&vf, -1)*2;
        
        *bits = (ALsizei) 16; //ov_read returns PCM 16-bit little-endian samples

        int channels = vi->channels;        
        if (channels == 1)
        {
            *format = AL_FORMAT_MONO16;
        } else if (channels == 2)
        {
            *format = AL_FORMAT_STEREO16;
        } else
        {
            return AL_FALSE;
        }
    }

    char * pcm_data = new char[*size];
    char * pcm_data_p = pcm_data;


    while(!eof)
    {
      ret=ov_read(&vf,(char *)pcm_data_p,4096,0,2,1,&current_section);
        if (ret == 0)
        {
            /* EOF */
            eof=1;
        } else if (ret < 0)
        {
            /* error in the stream.  Not a problem, just reporting it in
               case we (the app) cares.  In this case, we don't. */
            return AL_FALSE;
        } else
        {
            /* we don't bother dealing with sample rate changes, etc, but
               you'll have to*/

            pcm_data_p += ret;
        
        }
    }


    /* cleanup */
    ov_clear(&vf);
    
    *wave = pcm_data;

    return AL_TRUE;
}

void unloadOgg(ALenum format,
		   ALvoid *data,
		   ALsizei size,
		   ALsizei freq)
{
    delete [] (ALbyte*)data;
}

