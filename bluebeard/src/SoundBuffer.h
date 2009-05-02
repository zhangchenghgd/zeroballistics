
#ifndef BLUEBEARD_SOUNDBUFFER_INCLUDED
#define BLUEBEARD_SOUNDBUFFER_INCLUDED

#include <string>

#include <AL/alut.h>


//------------------------------------------------------------------------------
/**
 *  Stores some data about the sound. This is used to determine
 *  whether a sound with the same properties and filename already
 *  exists.
 */
class SoundBuffer
{
public:
    SoundBuffer(const std::string & filename);
    virtual ~SoundBuffer();

    const ALuint getId() const;

private:

    void loadBufferFromFile(const std::string & filename);

    ALuint    id_  ;        ///< The OpenAL buffer name.
    ALsizei   size_  ;      ///< Size in bytes
    ALsizei   freq_  ;      ///< Sampling frequency
    ALsizei   bits_  ;      ///< Bits per sample
    ALenum    format_;      ///< mono/stereo and bits. Can be AL_FORMAT_MONO8 - AL_FORMAT_STEREO16
    
    std::string filename_;  ///< The name of the sound file that was loaded into the buffer

    unsigned char flags_;   ///< The creation flags of the buffer.
    
};
#endif // #ifndef BLUEBEARD_SOUNDBUFFER_INCLUDED
