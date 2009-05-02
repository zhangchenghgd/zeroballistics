
#ifndef BLUEBEARD_SOUNDMANAGER_INCLUDED
#define BLUEBEARD_SOUNDMANAGER_INCLUDED

#include <iostream>
#include <vector>
#include <string>
#include <map>

#include <AL/alut.h>
#include <osg/Node>
#include <osg/NodeVisitor>


#include "Vector.h"
#include "Matrix.h"
#include "Exception.h"
#include "Log.h"
#include "ResourceManager.h"
#include "SoundBuffer.h"
#include "RegisteredFpGroup.h"

const unsigned MAX_NUMBER_SOURCES = 64;

const float DEFAULT_SOUND_REF_DIST = 3.5f;
const float DEFAULT_ROLLOF_FACTOR  = 1.0f;


class SoundSource;
class RigidBody;

//------------------------------------------------------------------------------
struct SoundListenerInfo
{
    SoundListenerInfo() :
        position_(0,0,0), velocity_(0,0,0), direction_(0,0,-1),
        up_(0,1,0), gain_(0.0) {}

    
    Vector position_;
    Vector velocity_;
    Vector direction_;
    Vector up_;
    float gain_;

};


//------------------------------------------------------------------------------
enum SOUND_SOURCE_CREATION_FLAGS
{
    SSCF_LOOP     =  1
};


#define s_soundmanager Loki::SingletonHolder<SoundManager, Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()
//------------------------------------------------------------------------------
class SoundManager : public ResourceManager<SoundBuffer>
{
    DECLARE_SINGLETON(SoundManager);

 public:
    typedef std::map<unsigned, std::string> SoundDeviceMap;

    virtual ~SoundManager();
    
    void setListenerTransform(const Matrix & transform);
    void setListenerVelocity(const Vector & vec);
    void setListenerGain(float volume);
    const SoundListenerInfo & getListenerInfo() const;

    void checkForErrors(const std::string & context);
    void toggleSoundEnabled();
    bool isSoundEnabled();


    bool existsDevice() const;

    ALuint getFreeAlSoundSource();
    void releaseAlSoundSource(ALuint id);

    const SoundDeviceMap & getAvailableSoundDevices() const;

    SoundSource * playSimpleEffect (const std::string & name,
                                    const Vector & pos, 
                                    float rolloff_factor = 1.0f);
    SoundSource * playLoopingEffect(const std::string & name, const RigidBody * body);
    
 protected:    
    void initSoundDevices();
    void createALSoundSources();

    std::string getALCErrorString(ALenum err);
    
    SoundDeviceMap available_sound_devices_;

    SoundListenerInfo listener_;
    
    typedef std::map<ALuint, bool> SoundSourceMap;
    SoundSourceMap sound_src_in_use_;

    ALCdevice  * current_device_;
    ALCcontext * current_context_;

    bool sounds_enabled_;

    RegisteredFpGroup fp_group_;
};

#endif // #ifndef BLUEBEARD_SOUNDMANAGER_INCLUDED
