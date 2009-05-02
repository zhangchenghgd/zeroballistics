
#ifndef BLUEBEARD_SOUNDSOURCE_INCLUDED
#define BLUEBEARD_SOUNDSOURCE_INCLUDED

#include <vector>
#include <string>

#include <osg/Node>
#include <AL/alut.h>

#include "Vector.h"
#include "Exception.h"
#include "Log.h"
#include "SceneManager.h"

//------------------------------------------------------------------------------
/**
 *  This class defines an OpenAL SoundSource as an osg::Node, which can be
 *  placed inside a SceneGraph, callbacks can be attached to the node and
 *  various SoundSource related attributes can be set.
 */
class SoundSource : public osg::Node
{
public:
    SoundSource();
    SoundSource(const std::string & filename, unsigned char flags = 0);

    void setPosition(const Vector & pos);
    void setVelocity(const Vector & vel);
    void setPitch(float pitch);
    void setGain(float gain);
    void setReferenceDistance(float ref_dist); 
    void setRolloffFactor(float factor);
    void setRelativePosition(bool rel);
    void setLooping(bool loop);

    bool isFinishedPlaying();

    const ALuint getId() const;
    const Vector & getPosition() const;
    float getPitch();
    float getGain();

    void play();

    void enable(bool enable);

    virtual const char * className () const { return "SoundSource"; }

    void addUpdateCallback(osg::NodeCallback * nc);

protected:

    ~SoundSource();

    void clearAlSource();

    ALuint      id_;        ///< The OpenAL source name
    ALuint      buffer_;    ///< The buffer containing the sound data
    Vector      position_;  ///< 3D Position of Sound Source
    Vector      velocity_;  ///< 3D Vector of Source Velocity
    ALfloat     gain_;      ///< float value for gain (def. 1.0f)
    ALfloat     pitch_;     ///< float value for pitch (def. 1.0f)
    ALfloat     rolloff_factor_; ///< see setRolloff description
    ALboolean   relative_;  ///< pos relative to listener
    ALboolean   loop_;      ///< loop sound or not
    ALfloat     reference_distance_; ///< is used for distance attenuation calculations
    bool        enabled_;
};

#endif // #ifndef BLUEBEARD_SOUNDSOURCE_INCLUDED
