
#include "SoundSource.h"

#include "SoundManager.h"
#include "SoundBuffer.h"
#include "UtilsOsg.h"
#include "RigidBodyVisual.h"

//------------------------------------------------------------------------------
SoundSource::SoundSource() :  // dummy source
    id_(0),
    buffer_(0),
    velocity_(Vector(0,0,0)),
    gain_(1.0f),
    pitch_(1.0f),
    rolloff_factor_(1.0f),
    relative_(false),
    loop_(false),
    reference_distance_(DEFAULT_SOUND_REF_DIST),
    should_play_(false),
    del_when_done_(true)
{
    // enforce update traversal
    setUpdateCallback(new osg::NodeCallback());
}

//------------------------------------------------------------------------------
SoundSource::SoundSource(const std::string & filename, unsigned char flags) :
    id_(0),
    buffer_(0),
    velocity_(Vector(0,0,0)),
    gain_(1.0f),
    pitch_(1.0f),
    rolloff_factor_(1.0f),
    relative_(false),
    loop_(false),
    reference_distance_(DEFAULT_SOUND_REF_DIST),
    should_play_(false),
    del_when_done_(true)
{
    s_log << Log::debug('S')
          << getName()
          << " constructor\n";
    
    // enforce update traversal
    setUpdateCallback(new osg::NodeCallback());
    
    // get buffer for this source
    const SoundBuffer * snd_buf = s_soundmanager.getResource(filename);

    // bail if buffer not ok
    if(!snd_buf || !snd_buf->getId()) return;

    buffer_ = snd_buf->getId();
    loop_   = (bool)(flags & SSCF_LOOP);

    setName(std::string("snd_src_") + filename);
}

//------------------------------------------------------------------------------
SoundSource::SoundSource(const SoundSource & s, const osg::CopyOp & op) :
    osg::MatrixTransform(s, op),
    id_(0),
    buffer_(s.buffer_),
    velocity_(s.velocity_),
    gain_(s.gain_),
    pitch_(s.pitch_),
    rolloff_factor_(s.rolloff_factor_),
    relative_(s.relative_),
    loop_(s.loop_),
    reference_distance_(s.reference_distance_),
    should_play_(false),
    del_when_done_(false)
{
    s_log << Log::debug('S')
          << getName()
          << " copy constructor\n";

    // enforce update traversal
    setUpdateCallback(new osg::NodeCallback());
}

//------------------------------------------------------------------------------
/**
 *  Do housekeeping tasks:
 *
 *  -) Set AL source position.
 *
 *  -) If we are waiting for a free source, check whether there is one
 *  available yet.
 *
 *  -) If we have finished playing, commit suicide.
 *
 *  -) Check & respect sound enabled state.
 *
 *  -) Check whether to cull source based on distance to listener.
 *
 *  -) Set sound source velocity
 */
void SoundSource::traverse(osg::NodeVisitor & nv)
{
    Node::traverse(nv);

    Vector pos = vecOsg2Gl(s_scene_manager.getWorldCoords(this).getTrans());

    if (nv.getVisitorType() == osg::NodeVisitor::CULL_VISITOR && id_)
    {
        alSourcefv(id_, AL_POSITION, (ALfloat*)&pos);

//         osgUtil::CullVisitor * cv = (osgUtil::CullVisitor*)&nv;
//         setPosition(vecOsg2Gl(cv->getModelViewMatrix()->getTrans()));
        
    } else if (nv.getVisitorType() == osg::NodeVisitor::UPDATE_VISITOR)
    {
        if (hasFinishedPlaying())
        {
            if (del_when_done_)
            {
                s_log << Log::debug('S')
                      << getName()
                      << " has finished playing -> deleting.\n";
                s_scene_manager.scheduleNodeForDeletion(this);
            } else
            {
                play(false);
            }
            return;
        }

        if (!s_soundmanager.isSoundEnabled())
        {
            internalPlay(false);
            return;
        }        
        
        // check if sound should be enabled based on distance
        if (isCulled(pos))
        {
            if (!loop_ && del_when_done_)
            {
                s_log << Log::debug('S')
                      << "Suiciding "
                      << getName()
                      << " because it's culled and non-looping\n";
                s_scene_manager.scheduleNodeForDeletion(this);
            } else if (isAlSourcePlaying())
            {
                s_log << Log::debug('S')
                      << "Stopping "
                      << getName()
                      << " because it's culled\n";
                internalPlay(false);
            }
        } else 
        {
            // reactivate looping sound sources
            internalPlay(should_play_);
        }


        if (nv.getUserData())
        {
            UpdateVisitorUserData * ud = dynamic_cast<UpdateVisitorUserData*>(nv.getUserData());
            assert(ud);
            setVelocity(ud->getVelocity());
        } else setVelocity(Vector(0,0,0));
    }
}



//------------------------------------------------------------------------------
void SoundSource::setPosition (const Vector &pos)
{
    osg::Matrix mat;
    mat.setTrans(pos.x_, pos.y_, pos.z_);
    setMatrix(mat);
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

    // second chance to start sounds culled before...
    if (relative_ && should_play_) internalPlay(true);

    if (!id_) return;

    alSourcei (id_, AL_SOURCE_RELATIVE, rel);
}


//------------------------------------------------------------------------------
void SoundSource::setLooping(bool loop)
{
    loop_ = loop;
    if (id_) alSourcei(id_, AL_LOOPING, loop);
}

//------------------------------------------------------------------------------
bool SoundSource::isLooping() const
{
    return loop_;
}


//------------------------------------------------------------------------------
bool SoundSource::hasFinishedPlaying()
{
    if (!buffer_)
    {
        s_log << Log::debug('S')
              << getName()
              << " has no buffer-> finished playing\n";
        return true;
    }
    
    if (loop_) return false;

    return !isAlSourcePlaying();
}

//------------------------------------------------------------------------------
const ALuint SoundSource::getId() const
{
    return id_;
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
void SoundSource::play(bool p)
{
    if (!buffer_) return;

    should_play_ = p;

    if (p)
    {
        Vector pos = vecOsg2Gl(s_scene_manager.getWorldCoords(this).getTrans());
        if (!isCulled(pos))
        {
            internalPlay(true);
        } else
        {
            s_log << Log::debug('S')
                  << "not starting "
                  << getName()
                  << " because it is culled.\n";
        }
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
          << getName()
          << " destructor\n";

    releaseAlSource(); 
}

//------------------------------------------------------------------------------
bool SoundSource::isAlSourcePlaying() const
{
    if (!id_) return false;
    
    int p;
    alGetSourcei(id_, AL_SOURCE_STATE, &p);
    return p == AL_PLAYING;
}


//------------------------------------------------------------------------------
/**
 *  Doesn't affect should_play_.
 */
void SoundSource::internalPlay(bool p)
{
    assert(buffer_);
    
    if (s_soundmanager.isSoundEnabled() && p && (id_ || acquireAlSource()))
    {
        if (!isAlSourcePlaying())
        {
            s_log << Log::debug('S')
                  << "playing "
                  << getName()
                  << "\n";
            alSourcePlay(id_);
        }
    } else if (id_)
    {
        // cannot play for any reason, or simply stopped. release our
        // al source for somebody else to use.
        s_log << Log::debug('S')
              << "stopping "
              << getName()
              << "\n";
        releaseAlSource();
    }
}


//------------------------------------------------------------------------------
bool SoundSource::acquireAlSource()
{
    if (id_) return true;
    
    id_ = s_soundmanager.getFreeAlSoundSource();    
    if (!id_) return false;
    
    // necessary: re-set all values so that the 
    // source can be played normally
    alSourcei(id_, AL_BUFFER,  buffer_);
    alSourcei(id_, AL_LOOPING, loop_);
    setGain(gain_);
    setPitch(pitch_);
    setVelocity(velocity_);
    setReferenceDistance(reference_distance_);
    setRolloffFactor(rolloff_factor_);
    setRelativePosition((bool)relative_);

    return true;
}


//------------------------------------------------------------------------------
void SoundSource::releaseAlSource()
{
    // only clear in OpenAL state machine if SoundSource was valid
    if(id_ == 0) return;

    if(isAlSourcePlaying())
    {
        alSourceStop(id_);
    }

    // clear to default values because alSource will be reused
    Vector v(0,0,0);
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


//------------------------------------------------------------------------------
bool SoundSource::isCulled(const Vector & pos) const
{
    return !relative_ &&
        ((s_soundmanager.getListenerInfo().position_ - pos).length() >
         s_params.get<float>("client.sound.cull_distance"));    
}

