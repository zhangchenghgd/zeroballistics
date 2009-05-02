
#include "SoundManager.h"

#include "SoundSource.h"
#include "ParameterManager.h"
#include "GameObjectVisual.h"
#include "SoundSourceCallbacks.h"
#include "OsgNodeWrapper.h"

// method of device enumeration, ALC_ENUMERATE_ALL_EXT seems
// to be implemented only on win platforms
#ifdef _WIN32
 #define DEVICES_SPECIFIER              ALC_ALL_DEVICES_SPECIFIER
 #define DEFAULT_DEVICES_SPECIFIER      ALC_DEFAULT_ALL_DEVICES_SPECIFIER
#else
 #define DEVICES_SPECIFIER              ALC_DEVICE_SPECIFIER
 #define DEFAULT_DEVICES_SPECIFIER      ALC_DEFAULT_DEVICE_SPECIFIER
#endif


//------------------------------------------------------------------------------
SoundManager::SoundManager() : 
    ResourceManager<SoundBuffer>("sounds"),
    current_device_(NULL),
    current_context_(NULL),
    sounds_enabled_(false)
{
    s_console.addFunction("toggleSoundEnabled",
                          Loki::Functor<void>(this, &SoundManager::toggleSoundEnabled),
                          &fp_group_);

    alutInitWithoutContext(NULL,NULL);

    try
    {
        initSoundDevices();
        createALSoundSources();
    } catch (Exception & e)
    {
        e.addHistory("SoundManager::SoundManager");
        current_context_ = NULL;
        current_device_  = NULL;
        s_log << e << "\n";
    }
}


//------------------------------------------------------------------------------
SoundManager::~SoundManager()
{
    if (!current_device_) return;

    // actually free all allocated Sound Sources
    ALuint src_id;
    SoundSourceMap::iterator it;
    for(it = sound_src_in_use_.begin();it != sound_src_in_use_.end(); it++)
    {
        src_id = it->first;
        alDeleteSources(1, &src_id); // delete sound source
    }
    sound_src_in_use_.clear();

    // Free Sound Buffers as long as the context is still valid
    unloadAllResources();

    alcMakeContextCurrent(NULL);
    alcDestroyContext(current_context_);
    alcCloseDevice   (current_device_);
    current_context_ = NULL;
    current_device_  = NULL;
  
    alutExit();
}


//------------------------------------------------------------------------------
void SoundManager::setListenerTransform(const Matrix & transform)
{
    // check if a sound device has been initialized
    if(!current_device_)
    {
        return;
    }

    listener_.position_  = transform.getTranslation();
    listener_.direction_ = -transform.getZ();
    listener_.up_        = transform.getY();

    Vector orientation[2];
    orientation[0] = listener_.direction_;
    orientation[1] = listener_.up_;

    alListenerfv(AL_ORIENTATION, (ALfloat*)&orientation);
    alListenerfv(AL_POSITION, (float*)&listener_.position_);
}

//------------------------------------------------------------------------------
void SoundManager::setListenerVelocity(const Vector & vec)
{
    // check if a sound device has been initialized
    if(!current_device_)
    {
        return;
    }

    listener_.velocity_ = vec;
    alListenerfv(AL_VELOCITY, (float*)&vec);
}

//------------------------------------------------------------------------------
void SoundManager::setListenerGain(float volume)
{
    // check if a sound device has been initialized
    if(!current_device_)
    {
        return;
    }

    listener_.gain_ = volume;
    alListenerf(AL_GAIN, (ALfloat)volume);
}

//------------------------------------------------------------------------------
const SoundListenerInfo & SoundManager::getListenerInfo() const
{
    return listener_;
}

//------------------------------------------------------------------------------
void SoundManager::checkForErrors(const std::string & context)
{
    ALenum errorNr;

    
    // check for alut errors
    if ((errorNr = alutGetError ()) != ALUT_ERROR_NO_ERROR)
    {
        s_log << Log::error << "Sound ALut: " << alutGetErrorString(errorNr)
              << "\n\tcontext: "
              << context
              << "\n";
    }

    // check for AL errors
    if ((errorNr = alGetError())  != AL_NO_ERROR)
    {
        s_log << Log::error << "Sound OpenAL: " << alGetString(errorNr)
              << "\n\tcontext: "
              << context
              << "\n";
    }

    // check for ALC errors
    if ((errorNr = alcGetError(current_device_)) != ALC_NO_ERROR)
    {
        s_log << Log::error << "Sound OpenAL Context: " << getALCErrorString(errorNr)
              << "\n\tcontext: "
              << context
              << "\n";
    }

}

//------------------------------------------------------------------------------
void SoundManager::toggleSoundEnabled()
{
    sounds_enabled_ = !sounds_enabled_;
}

//------------------------------------------------------------------------------
bool SoundManager::isSoundEnabled()
{
    return sounds_enabled_;
}


//------------------------------------------------------------------------------
bool SoundManager::existsDevice() const
{
    return current_device_;
}

//------------------------------------------------------------------------------
/**
 *  \brief returns true and gives the id of a free sound source or
 *  returns false if no free sound source was available
 */
ALuint SoundManager::getFreeAlSoundSource()
{
    SoundSourceMap::iterator it;

    for(it = sound_src_in_use_.begin();it != sound_src_in_use_.end(); it++)
    {
        if(!it->second) // if sound source is free
        {
            it->second = true;
            return it->first;
        }
    }

    s_log << Log::debug('S') << "Tried to create Sound Source, but no free Sources were available!!\n";
    return 0;
}

//------------------------------------------------------------------------------
const SoundManager::SoundDeviceMap & SoundManager::getAvailableSoundDevices() const
{
    return available_sound_devices_;
}

//------------------------------------------------------------------------------
/**
 *  \brief "frees" a sound source inside sound manager to make it available
 *  for use again.
 */
void SoundManager::releaseAlSoundSource(ALuint id)
{
    SoundSourceMap::iterator it = sound_src_in_use_.find(id);
    if (it != sound_src_in_use_.end()) 
    {
        it->second = false;
    } else
    {
        s_log << Log::warning
              << "SoundManager::releaseAlSoundSource: unknown source "
              << id
              << "\n";
    }
}


//------------------------------------------------------------------------------
SoundSource * SoundManager::playSimpleEffect(const std::string & name,
                                             const Vector & pos,
                                             float rolloff_factor)
{
    SoundSource * effect;
    
    if(Vector(listener_.position_ - pos).length() >
       s_params.get<float>("client.sound.cull_distance") ||
        !sounds_enabled_)
    {
        effect = new SoundSource(); // Create dummy sound source, will
                                    // be deleted because it has no
                                    // buffer
    } else
    {
        effect = new SoundSource(name);
    }

    effect->setRolloffFactor(rolloff_factor);
    effect->setPosition(pos);
    effect->addUpdateCallback(new DeleteSoundSourceAfterPlaying());
    s_scene_manager.getRootNode()->addChild(effect);
    effect->play();

    return effect;
}


//------------------------------------------------------------------------------
SoundSource * SoundManager::playLoopingEffect(const std::string & name,
                                              const RigidBody * body)
{
    SoundSource * effect = new SoundSource(name, SSCF_LOOP);
    effect->setPosition(body->getTransform().getTranslation());
    // In case looping stops (e.g. machine gun)
    effect->addUpdateCallback(new DeleteSoundSourceAfterPlaying());

    assert(body->getUserData());
    assert(((GameObjectVisual*)body->getUserData())->getWrapperNode());
    
    osg::Group * group = ((GameObjectVisual*)body->getUserData())->getWrapperNode()->getOsgNode();
    group->addChild(effect);
    effect->play();

    return effect;
}


//------------------------------------------------------------------------------
/**
 *  \brief Initializes a sound device selected with device_index from config file
 *  Also prints out all devices available if parameter list_available_devices is set
 */
void SoundManager::initSoundDevices()
{
    char *devices = (char *)alcGetString(NULL, DEVICES_SPECIFIER);

    // openal 0.8 - device enumeration is not implemented...
    // ignore error, default device will be selected afterwards
    ALenum error = alcGetError(NULL);
    if (error != ALC_INVALID_DEVICE && error != ALC_NO_ERROR)
    {
        s_log << Log::error << "Sound OpenAL Context: " << getALCErrorString(error)
              << "\n\tcontext: SoundManager::initSoundDevices\n";
    }

    
    // copy for use below (alcGetString returns the full list only once)
    char *devices_selection = devices;
    bool device_initialized = false;


    const char *actual_device_name;
    unsigned index = 0;
    std::string actual_devices;
    
    while (*devices != '\0') 
    {
        // try to open device
        ALCdevice *device = alcOpenDevice(devices);
        if(device != NULL)
        {
            // read device info for usable devices
            actual_device_name = alcGetString(device, DEVICES_SPECIFIER);

            actual_devices += "OpenAL device available: " +
                toString(actual_device_name) + " with device index: " + toString(index) + "\n";

            // store in map for available sound devices, used in options menu
            available_sound_devices_.insert(std::make_pair(index, toString(actual_device_name)));

            index++;

            alcCloseDevice(device);
        }
        
        // get next device from devices
        devices += strlen(devices) + 1;
         
    }

    // if a device has been manually selected with index number
    // try to open the selected device and create a context for the device
    index = 0;
    while (*devices_selection != '\0')
    {
        ALCdevice *device = alcOpenDevice(devices_selection);
        if(device != NULL)
        {
            // if device is the selected device, open context
            if(index == s_params.get<unsigned>("client.sound.device_index"))
            {
                ALCcontext *context = alcCreateContext(device, NULL);
                if (context)
                {
                    alcMakeContextCurrent(context);  
                    device_initialized  = true;
                    break;
                }
            }
            index++;
        } 
        devices_selection += strlen(devices_selection) + 1;         
    }

    // if no devices have been found, try to make a fallback to default device
    if(actual_devices.empty() && device_initialized == false)   
    {
        devices = (char *)alcGetString(NULL, DEFAULT_DEVICES_SPECIFIER);
        ALCdevice *device = alcOpenDevice(devices);
        if(device != NULL)
        {
            ALCcontext *context = alcCreateContext(device, NULL);
            if (context)
            {
                alcMakeContextCurrent(context);  
                device_initialized  = true;

                s_log << "Fallback to default sound device.\n";
                available_sound_devices_.insert(std::make_pair(0, toString(alcGetString(device, DEVICES_SPECIFIER))));
            }
        }
    }


    checkForErrors("after initializing devices");
    
    if(!device_initialized && actual_devices.empty())
    {
        s_log << Log::error << "OpenAL was not able to find any sound device on this computer!!\n";
        return;
    }


    // print device info    
    if(s_params.get<bool>("client.sound.list_available_devices"))
    {
        s_log << actual_devices;
    }


    // if no device could be initialized until now, something is terribly wrong
    // either an OpenAL error or the given device_index was wrong
    if(!device_initialized)
    {
        s_log << Log::error << "Error initializing sound device, check for OpenAL error or invalid device index!\n";
        return;        
    }

    current_context_ = alcGetCurrentContext();
    current_device_ = alcGetContextsDevice(current_context_);
    sounds_enabled_ = s_params.get<bool>("client.sound.enabled");

    // list all available device info for the currently selected device
    if(s_params.get<bool>("client.sound.show_device_info"))
    {
        int major, minor;
        actual_device_name = alcGetString(current_device_, DEVICES_SPECIFIER);
	    alcGetIntegerv(current_device_, ALC_MAJOR_VERSION, sizeof(int), &major);
	    alcGetIntegerv(current_device_, ALC_MINOR_VERSION, sizeof(int), &minor);

        s_log << " Sound device used: "
              << actual_device_name << ", Version: " << major << "." << minor <<"\n";
        s_log << " Sound device vendor: "
              << alGetString(AL_VENDOR) << "\n";
        s_log << " Sound device renderer: "
              << alGetString(AL_RENDERER) << "\n";
        s_log << " Sound device version: "
              << alGetString(AL_VERSION) << "\n";
    
        const char* exten = alcGetString(current_device_, ALC_EXTENSIONS);
        if(exten != NULL)
        {
            s_log << " Sound device (context specific) extensions: " << exten << "\n";
        }

        s_log << " Sound device available extensions: " << alGetString(AL_EXTENSIONS) << "\n";
    }
}

//------------------------------------------------------------------------------
/**
 *  \brief  this method creates the maximum number of sound sources 
 *          available on the target implementation and populates a
 *          map that further on manages free sound sources.
 *          This is done to avoid generating OpenAL sources on the 
 *          fly, because this may lead to OpenAL troubles.
 */
void SoundManager::createALSoundSources()
{
    const unsigned MIN_NUMBER_SOUCRES_WARNING = 8;

    checkForErrors("SoundManager::createALSoundSources");

    // try to create as much sources as possible
    for(unsigned s=0; s<MAX_NUMBER_SOURCES; s++)
    {
        ALuint src_id = 0;
        alGenSources(1, &src_id);

        if ((alGetError() == AL_NO_ERROR) && (alIsSource(src_id)))
        {           
            sound_src_in_use_.insert(std::make_pair(src_id, false));
            continue;
        }

        // sound source creation went wrong, stop creating
        break;
        
    }

    if(sound_src_in_use_.size() < MIN_NUMBER_SOUCRES_WARNING)
    {
        s_log   << Log::warning << " There are less than " 
                << MIN_NUMBER_SOUCRES_WARNING
                << " sound sources available! Available sources: "
                << sound_src_in_use_.size() << "\n";
    }
    else
    {
        s_log << Log::debug('i') << "Available Sound Sources: " << sound_src_in_use_.size() << "\n";
    }

    if(sound_src_in_use_.empty())
    {
        throw Exception("Unable to create any OpenAL Sound Source!!");
    }

}



//------------------------------------------------------------------------------
std::string SoundManager::getALCErrorString(ALenum err)
{
    switch(err)
    {
        case ALC_NO_ERROR:
            return std::string("AL_NO_ERROR");
            break;

        case ALC_INVALID_DEVICE:
            return std::string("ALC_INVALID_DEVICE");
            break;

        case ALC_INVALID_CONTEXT:
            return std::string("ALC_INVALID_CONTEXT");
            break;

        case ALC_INVALID_ENUM:
            return std::string("ALC_INVALID_ENUM");
            break;

        case ALC_INVALID_VALUE:
            return std::string("ALC_INVALID_VALUE");
            break;

        case ALC_OUT_OF_MEMORY:
            return std::string("ALC_OUT_OF_MEMORY");
            break;
        default:
            return std::string("unkown ALC Error");
    }
}
