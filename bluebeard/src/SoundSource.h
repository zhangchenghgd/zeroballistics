
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
class SoundSource : public osg::MatrixTransform
{
public:
    META_Node(bluebeard, SoundSource);

    SoundSource();
    SoundSource(const std::string & filename, unsigned char flags = 0);
    SoundSource(const SoundSource & s, const osg::CopyOp & op);

    virtual void traverse(osg::NodeVisitor & nv);
        
    void setPosition(const Vector & pos);
    void setVelocity(const Vector & vel);
    void setPitch(float pitch);
    void setGain(float gain);
    void setReferenceDistance(float ref_dist); 
    void setRolloffFactor(float factor);
    void setRelativePosition(bool rel);
    void setLooping(bool loop);
    bool isLooping() const;

    bool hasFinishedPlaying();

    const ALuint getId() const;

    float getPitch();
    float getGain();

    void play(bool p = true);

    void addUpdateCallback(osg::NodeCallback * nc);

protected:

    ~SoundSource();

    bool isAlSourcePlaying() const;

    void internalPlay(bool p);
    
    bool acquireAlSource();
    void releaseAlSource();

    bool isCulled(const Vector & pos) const;

    ALuint      id_;        ///< The OpenAL source name
    ALuint      buffer_;    ///< The buffer containing the sound data
    Vector      velocity_;  ///< 3D Vector of Source Velocity
    ALfloat     gain_;      ///< float value for gain (def. 1.0f)
    ALfloat     pitch_;     ///< float value for pitch (def. 1.0f)
    ALfloat     rolloff_factor_; ///< see setRolloff description
    ALboolean   relative_;  ///< pos relative to listener
    ALboolean   loop_;      ///< loop sound or not
    ALfloat     reference_distance_; ///< is used for distance attenuation calculations

    bool should_play_;

    bool del_when_done_;
};

#endif // #ifndef BLUEBEARD_SOUNDSOURCE_INCLUDED
