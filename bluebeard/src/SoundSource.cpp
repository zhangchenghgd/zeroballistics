
#include "SoundSource.h"

#include "SoundManager.h"
#include "SoundBuffer.h"
#include "SoundSourceCallbacks.h"


//------------------------------------------------------------------------------
SoundSource::SoundSource() :  // dummy source
    id_(0),
    buffer_(0),
    position_(Vector(0,0,0)),
    velocity_(Vector(0,0,0)),
    gain_(1.0f),
    pitch_(1.0f),
    rolloff_factor_(1.0f),
    relative_(false),
    loop_(false),
    reference_distance_(DEFAULT_SOUND_REF_DIST),
    enabled_(false),
    waiting_for_source_(false)
{
}

//------------------------------------------------------------------------------
SoundSource::SoundSource(const std::string & filename, unsigned char flags) :
    id_(0),
    buffer_(0),
    position_(Vector(0,0,0)),
    velocity_(Vector(0,0,0)),
    gain_(1.0f),
    pitch_(1.0f),
    rolloff_factor_(1.0f),
    relative_(false),
    loop_(false),
    reference_distance_(DEFAULT_SOUND_REF_DIST),
    enabled_(false),
    waiting_for_source_(false)
{
    // get buffer for this source
    const SoundBuffer * snd_buf = s_soundmanager.getResource(filename);

    // bail if buffer not ok
    if(!snd_buf || !snd_buf->getId()) return;

    buffer_ = snd_buf->getId();
    loop_   = (bool)(flags & SSCF_LOOP);

    id_ = s_soundmanager.getFreeAlSoundSource();
    
    setName(std::string("snd_src_") + filename);

    if (id_)
    {
        // source is only enabled if a free AL source is found
        // otherwise it's disabled so it get's deleted by updatecallback
        enabled_ = true;

        alSourcei(id_, AL_BUFFER,  buffer_);
        alSourcei(id_, AL_LOOPING, loop_);

        addUpdateCallback(new CullDistantSource());
    }
    else  
    {
        // if this is a looping sound, that has not got a source yet
        // retry to get a source until one is freed
        if(loop_)
        {
            waiting_for_source_ = true;
            addUpdateCallback(new CullDistantSource());
        }
    }
}

//------------------------------------------------------------------------------
void SoundSource::setPosition (const Vector &pos)
{
    //copy position data
    position_ = pos;

    if (!id_) return;
    alSourcefv(id_, AL_POSITION, (ALfloat*)&position_);
}

//------------------------------------------------------------------------------
void SoundSource::setVelocity (const Vector &vel)
{
    //copy velocity data
    velocity_ = vel;

    if (!id_) return;
    alSourcefv(id_, AL_VELOCITY, (ALfloat*)&velocity_);
}

//------------------------------------------------------------------------------
void SoundSource::setPitch(float pitch)
{
    assert(!equalsZero(pitch));
    pitch_ = pitch;

    if (!id_) return;
    alSourcef(id_, AL_PITCH, pitch_);
}

//------------------------------------------------------------------------------
void SoundSource::setGain(float gain)
{
    gain_ = gain;

    if (!id_) return;
    alSourcef(id_, AL_GAIN, gain_);
}

//------------------------------------------------------------------------------
/***
 * This is used for distance attenuation calculations based on inverse distance 
 * with rolloff. Depending on the distance model it will also act as a distance 
 * threshold below which gain is clamped.
 **/
void SoundSource::setReferenceDistance(float ref_dist)
{
    reference_distance_ = ref_dist;

    if (!id_) return;

    alSourcef(id_, AL_REFERENCE_DISTANCE, ref_dist);
}

//------------------------------------------------------------------------------
/***
 * The distance under which the volume for the source would normally drop by half (before
 * being influenced by rolloff factor or AL_MAX_DISTANCE)
 ***/
void SoundSource::setRolloffFactor(float factor)
{
    rolloff_factor_ = factor;

    if (!id_) return;

    alSourcef (id_, AL_ROLLOFF_FACTOR,  factor);
}

//------------------------------------------------------------------------------
/***
*  determines if the positions are relative to the listener
**/
void SoundSource::setRelativePosition(const bool rel)
{
    relative_ = rel;

    if (!id_) return;

    alSourcei (id_, AL_SOURCE_RELATIVE, rel);
}


//------------------------------------------------------------------------------
void SoundSource::setLooping(bool loop)
{
    loop_ = loop;

    /// if looping is set to false (i.e. like in MG) then this
    /// source can be deleted and must not be waiting for a new source anymore
    if(loop == false) waiting_for_source_ = false;

    if (id_) alSourcei(id_, AL_LOOPING, loop);
}

//------------------------------------------------------------------------------
bool SoundSource::isLooping() const
{
    return loop_;
}

//------------------------------------------------------------------------------
void SoundSource::setWaitingForSource(bool waiting)
{
    waiting_for_source_ = waiting;
}

//------------------------------------------------------------------------------
bool SoundSource::isWaitingForSource() const
{
    return waiting_for_source_;
}

//------------------------------------------------------------------------------
bool SoundSource::isFinishedPlaying()
{
    if (!buffer_) return true; // Dummy sound source used for culled
                               // simple effects

    if (!enabled_ && !loop_) return true; // used in DeleteSoundSourceAfterPlaying
                                          // callback to delete culled non looping
                                          // sounds

    // If looping sounds get culled based on their distance, they
    // should not be removed.
    if (!id_) return false; 

    int tmp;
    alGetSourcei(id_, AL_SOURCE_STATE, &tmp);

    if(tmp == AL_PLAYING)
    {
        return false;
    } else
    {
        return true;
    }
}

//------------------------------------------------------------------------------
const ALuint SoundSource::getId() const
{
    return id_;
}

//------------------------------------------------------------------------------
const Vector & SoundSource::getPosition() const
{
    return position_;
}

//------------------------------------------------------------------------------
float SoundSource::getPitch()
{
    return pitch_;
}

//------------------------------------------------------------------------------
float SoundSource::getGain()
{
    return gain_;
}

//------------------------------------------------------------------------------
void SoundSource::play()
{
    if (id_ && s_soundmanager.isSoundEnabled()) alSourcePlay(id_);
}

//------------------------------------------------------------------------------
void SoundSource::enable(bool enable)
{
    if(enable == enabled_) return;

    // enable sounds, request source id, start playing paused sounds and update
    // values inside OpenAL source
    if(enable && !id_) 
    {
        id_ = s_soundmanager.getFreeAlSoundSource();
       
        if(id_)
        {
            // necessary: re-set all values so that the 
            // source can be played normally
            enabled_ = true;
            setGain(gain_);
            setPosition(position_);
            setPitch(pitch_);
            setVelocity(velocity_);
            setReferenceDistance(reference_distance_);
            setRolloffFactor(rolloff_factor_);
            setRelativePosition((bool)relative_);
            alSourcei(id_, AL_BUFFER,  buffer_);
            alSourcei(id_, AL_LOOPING, loop_);

            alSourcePlay(id_);
        }
    }

    // disable sounds, release source 
    if(!enable && id_) 
    {
        clearAlSource();  
    }
}

//------------------------------------------------------------------------------
void SoundSource::addUpdateCallback(osg::NodeCallback * nc)
{
    assert((bool)nc);

    osg::NodeCallback * current_nc = getUpdateCallback();
    if(current_nc)
        current_nc->addNestedCallback(nc);
    else
        setUpdateCallback(nc);
}

//------------------------------------------------------------------------------
SoundSource::~SoundSource()
{
    s_log << Log::debug('S') 
          << "SoundSource destructor: " 
          << getName() << "\n";

    clearAlSource(); 
}

//------------------------------------------------------------------------------
void SoundSource::clearAlSource()
{
    // only clear in OpenAL state machine if SoundSource was valid
    if(id_ > 0)
    {
        // if sound source was still playing, stop it
        int playing;
        alGetSourcei(id_, AL_SOURCE_STATE, &playing);

        if(playing == AL_PLAYING)
        {
            alSourceStop(id_);
        }

        // clear to default values because alSource will be reused
        Vector v(0,0,0);
        enabled_ = false;
        alSourcef (id_, AL_GAIN, 1.0f);
        alSourcefv(id_, AL_POSITION, (ALfloat*)&v);
        alSourcef (id_, AL_PITCH, 1.0f);
        alSourcefv(id_, AL_VELOCITY, (ALfloat*)&v);
        alSourcef (id_, AL_REFERENCE_DISTANCE, DEFAULT_SOUND_REF_DIST);
        alSourcef (id_, AL_ROLLOFF_FACTOR, 1.0f);
        alSourcei (id_, AL_SOURCE_RELATIVE, 0);
        alSourcei (id_, AL_BUFFER,  0);  // detach source from sound buffer
        alSourcei (id_, AL_LOOPING, false);

        s_soundmanager.releaseAlSoundSource(id_);
        id_ = 0;  
    }
}

