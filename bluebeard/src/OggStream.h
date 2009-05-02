
#ifndef BLUEBEARD_OGGSTREAM_INCLUDED
#define BLUEBEARD_OGGSTREAM_INCLUDED

#include <string>

#include <AL/alut.h>
#include <vorbis/vorbisfile.h>


#include "Scheduler.h"

const unsigned BUFFER_SIZE = 4096 * 12;

//------------------------------------------------------------------------------
/**
 *  Provides streaming output of ogg files
 *  alSource is retrieved from SoundManager
 */
class OggStream
{
public:

    OggStream(const std::string & filename, unsigned char flags = 0);
    ~OggStream();

    void loadBuffer(const std::string & filename);
    void release();

    bool play(bool fade_in);
    void pause(bool fade_out);

    bool isPaused();
    bool isPlaying();

    
protected:

    void updateStream(float dt);
    
    bool update();
    bool stream(ALuint buffer);

    void empty();
    void check(unsigned line);

    std::string oggError(int code);

    void fadeIn(float dt);
    void fadeOut(float dt);
    
private:

    std::string filename_;  ///< The name of the sound file that was loaded into the buffer
    OggVorbis_File stream_;
    vorbis_info * vorbis_info_;

    ALuint buffers_[2]; ///< front and back buffers
    ALuint source_id_;  ///< alSource id
    ALenum format_;
    unsigned char flags_;   ///< The creation flags of the buffer.

    hTask task_update_;
    hTask task_fade_;
    RegisteredFpGroup fp_group_;
};
#endif // #ifndef BLUEBEARD_OGGSTREAM_INCLUDED
