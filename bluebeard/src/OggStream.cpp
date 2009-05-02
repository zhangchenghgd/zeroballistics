
#include "OggStream.h"

#include "OggVorbis.h"
#include "Exception.h"
#include "Log.h"
#include "SoundManager.h"
#include "Paths.h"


float UPDATE_PERIOD  = 0.1f; // time between sound buffer updates
float FADE_PERIOD    = 0.1f; // time between volume adjustments when fading
float FADE_CHANGE    = 0.1f; // amount of volume change per adjustment when fading


//------------------------------------------------------------------------------
OggStream::OggStream(const std::string & filename, unsigned char flags) :
    filename_(filename),
    source_id_(0),
    format_(0),
    flags_(flags),
    task_update_(INVALID_TASK_HANDLE),
    task_fade_(INVALID_TASK_HANDLE)
{
    if (s_soundmanager.existsDevice())
    {
        try
        {
            loadBuffer(filename);
        } catch (Exception & e)
        {
            e.addHistory("OggStream::OggStream");
            check(__LINE__);
            throw e;
        }
    }
}

//------------------------------------------------------------------------------
OggStream::~OggStream()
{
    if(source_id_) release();   
}

//------------------------------------------------------------------------------
void OggStream::loadBuffer(const std::string & filename)
{
    int result;
    FILE * ogg_file = NULL;

    std::string total_filename = MUSIC_PATH + filename;

    if(!(ogg_file = fopen(total_filename.c_str(), "rb")))
        throw Exception("Could not open Ogg file:" + total_filename);

    if((result = ov_open(ogg_file, &stream_, NULL, 0)) < 0)
    {
        fclose(ogg_file);        
        throw Exception("Could not open Ogg stream: " + oggError(result));
    }

    vorbis_info_ = ov_info(&stream_, -1);

    if(vorbis_info_->channels == 1)
        format_ = AL_FORMAT_MONO16;
    else
        format_ = AL_FORMAT_STEREO16;
        
        
    alGenBuffers(2, buffers_);
    check(__LINE__);
    source_id_ = s_soundmanager.getFreeAlSoundSource();
    check(__LINE__);
    
    alSource3f(source_id_, AL_POSITION,        0.0, 0.0, 0.0);
    alSource3f(source_id_, AL_VELOCITY,        0.0, 0.0, 0.0);
    alSource3f(source_id_, AL_DIRECTION,       0.0, 0.0, 0.0);
    alSourcef (source_id_, AL_GAIN,            1.0);
    alSourcef (source_id_, AL_ROLLOFF_FACTOR,  0.0          );
    alSourcei (source_id_, AL_SOURCE_RELATIVE, AL_TRUE      );
}

//------------------------------------------------------------------------------
void OggStream::release()
{
    // if sound source was still playing, stop it
    int playing;
    alGetSourcei(source_id_, AL_SOURCE_STATE, &playing);

    if(playing == AL_PLAYING)
    {
        alSourceStop(source_id_);
    }
    
    empty();
    s_soundmanager.releaseAlSoundSource(source_id_);
    check(__LINE__);

    alDeleteBuffers(2, buffers_);
    check(__LINE__);

    ov_clear(&stream_);
}

//------------------------------------------------------------------------------
/**
 * This will start playing the Ogg. If the Ogg is already playing then there is no 
 * reason to do it again. Also initialize the buffers with their first data set. 
 * Then queue them and tell the source to play them.
 **/
bool OggStream::play(bool fade_in)
{
    if (fade_in)
    {
        if (task_fade_ != INVALID_TASK_HANDLE)
        {
            s_scheduler.removeTask(task_fade_, &fp_group_);
        }

        task_fade_ = s_scheduler.addTask(PeriodicTaskCallback(this, &OggStream::fadeIn),
                                         FADE_PERIOD,
                                         "OggStream::fadeIn",
                                         &fp_group_);
    } else alSourcef (source_id_, AL_GAIN, 1.0);



    if(isPlaying())
    {
        assert(task_update_ != INVALID_TASK_HANDLE);
        return true;
    }
    
    if(stream_.bittrack <= 0.0) // begin with playing condition
    {
        if(!stream(buffers_[0]))
            return false;
            
        if(!stream(buffers_[1]))
            return false;
        
        alSourceQueueBuffers(source_id_, 2, buffers_);
    }

    alSourcePlay(source_id_); // continue playing
    check(__LINE__);


    if (task_update_ == INVALID_TASK_HANDLE)
    {
        task_update_ = s_scheduler.addTask(PeriodicTaskCallback(this, &OggStream::updateStream),
                                           UPDATE_PERIOD,
                                           "OggStream::updateStream",
                                           &fp_group_);
    }
    
    return true;
}

//------------------------------------------------------------------------------
void OggStream::pause(bool fade_out)
{
    if (fade_out)
    {
        if (task_fade_ != INVALID_TASK_HANDLE)
        {
            s_scheduler.removeTask(task_fade_, &fp_group_);
        }
    
        task_fade_ = s_scheduler.addTask(PeriodicTaskCallback(this, &OggStream::fadeOut),
                                         FADE_PERIOD,
                                         "OggStream::fadeOut",
                                         &fp_group_);   
    } else
    {
        s_scheduler.removeTask(task_fade_,   &fp_group_);
        s_scheduler.removeTask(task_update_, &fp_group_);
        task_fade_   = INVALID_TASK_HANDLE;
        task_update_ = INVALID_TASK_HANDLE;
    
        alSourcePause(source_id_);
    }
}

//------------------------------------------------------------------------------
bool OggStream::isPaused()
{
    ALenum state;
    
    alGetSourcei(source_id_, AL_SOURCE_STATE, &state);
    
    return (state == AL_PAUSED);
}

//------------------------------------------------------------------------------
bool OggStream::isPlaying()
{
    ALenum state;
    
    alGetSourcei(source_id_, AL_SOURCE_STATE, &state);
    
    return (state == AL_PLAYING);
}

//------------------------------------------------------------------------------
void OggStream::updateStream(float dt)
{
    // in this case a buffer underrun is detected, source stops playing
    // so restart it
    if(!isPlaying()) play(false);

    // if source is paused on purpose, we don't need to update it
    if(!isPaused()) update();

}



//------------------------------------------------------------------------------
/**
 * SourceQueueBuffers in a nutshell: There is a 'list' of buffers. When you unqueue a buffer
 * it gets popped off of the front. When you queue a buffer it gets pushed to the back.
 *
 * Check if any buffers have already been played. If there is then start popping each of them 
 * off the back of the queue, refill the buffers with data from the stream, and then 
 * push them back onto the queue so that they can be played. 
 * The 'stream' function also tells us if the stream is finished playing. 
 * This flag is reported back when the function returns.
**/
bool OggStream::update()
{
    int processed;
    bool active = true;

    alGetSourcei(source_id_, AL_BUFFERS_PROCESSED, &processed);

    while(processed--)
    {
        ALuint buffer;
        
        alSourceUnqueueBuffers(source_id_, 1, &buffer);
        check(__LINE__);

        active = stream(buffer);

        alSourceQueueBuffers(source_id_, 1, &buffer);
        check(__LINE__);
    }    

    return active;
}

//------------------------------------------------------------------------------
/**
 * Fills the buffers with data from the Ogg bitstream. 
 * ov_read reads data from the Ogg bitstream. vorbisfile does all the decoding 
 * of the bitstream
 *
 * The return value of 'ov_read' indicates several things. If the value of the result 
 * is positive then it represents how much data was read. This is important because 
 * 'ov_read' may not be able to read the entire size requested (usually EOF). 
 * If the result of 'ov_read' is negative then it indicates that there was an error 
 * in the bitstream. The value of the result is an error code in this case. 
 * If the result happens to equal zero then there is nothing left in the file to play.
**/
bool OggStream::stream(ALuint buffer)
{
    char pcm[BUFFER_SIZE];
    unsigned size = 0;
    int  section;
    int  result;

    while(size < BUFFER_SIZE)
    {
        result = ov_read(&stream_, pcm + size, BUFFER_SIZE - size, 0, 2, 1, &section);
    
        if(result > 0)
            size += result;
        else
        {
            if(result < 0)
                throw Exception(oggError(result));
            else
            {
                if(flags_ == SSCF_LOOP)
                {
                    ov_raw_seek(&stream_,0);
                }
                else
                    break;
            }
        }
    }
    
    if(size == 0)
        return false;
        
    alBufferData(buffer, format_, pcm, size, vorbis_info_->rate);
    check(__LINE__);
    
    return true;
}

//------------------------------------------------------------------------------
void OggStream::empty()
{
    check(__LINE__);
    
    int queued;
    
    alGetSourcei(source_id_, AL_BUFFERS_QUEUED, &queued);

    check(__LINE__);
    
    while(queued--)
    {
        ALuint buffer;
    
        alSourceUnqueueBuffers(source_id_, 1, &buffer);
        check(__LINE__);
    }
}

//------------------------------------------------------------------------------
void OggStream::check(unsigned line)
{
    s_soundmanager.checkForErrors("OggStream l. " + toString(line));
}

//------------------------------------------------------------------------------
std::string OggStream::oggError(int code)
{
    switch(code)
    {
		case OV_FALSE:
			return std::string("Invalid or no OGG data available.");
        case OV_HOLE:
			return std::string("Vorbisfile encoutered missing or corrupt data in the bitstream.");
        case OV_EIMPL:
			return std::string("OpenAL OGG Feature not implemented.");
        case OV_EINVAL:
			return std::string("Either an invalid argument, or incomplete initialized argument passed to libvorbisfile call.");
		case OV_EBADLINK:
			return std::string("The given link exists in the Vorbis data stream, but is not decipherable due to garbage or corruption.");
        case OV_ENOSEEK:
			return std::string("Given stream is not seekable.");
		case OV_EREAD:
            return std::string("Read from media error.");
        case OV_ENOTVORBIS:
            return std::string("Not Vorbis data.");
        case OV_EVERSION:
            return std::string("Vorbis version mismatch.");
        case OV_EBADHEADER:
            return std::string("Invalid Vorbis header.");
        case OV_EFAULT:
            return std::string("Internal logic fault (bug or heap/stack corruption.");
        default:
            return std::string("Unknown Ogg error.");
    }
}

//------------------------------------------------------------------------------
void OggStream::fadeIn(float dt)
{
    ALfloat cur_gain;
    alGetSourcef(source_id_, AL_GAIN, &cur_gain);

    cur_gain += FADE_CHANGE;

    if (cur_gain >= 1.0f)
    {
        cur_gain = 1.0f;
        s_scheduler.removeTask(task_fade_, &fp_group_);
        task_fade_ = INVALID_TASK_HANDLE;
    }

    alSourcef (source_id_, AL_GAIN, cur_gain);

}


//------------------------------------------------------------------------------
void OggStream::fadeOut(float dt)
{
    ALfloat cur_gain;
    alGetSourcef(source_id_, AL_GAIN, &cur_gain);

    cur_gain -= FADE_CHANGE;

    if (cur_gain <= 0.0f)
    {
        pause(false);
    } else
    {
        alSourcef (source_id_, AL_GAIN, cur_gain);
    }
}
