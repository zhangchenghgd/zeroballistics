
#include "SoundBuffer.h"

#include "OggVorbis.h"
#include "Exception.h"
#include "Log.h"
#include "SoundManager.h"
#include "Paths.h"


//------------------------------------------------------------------------------
SoundBuffer::SoundBuffer(const std::string & filename) :
    id_(0),
    size_(-1),
    freq_(-1),
    bits_(-1),
    format_(0),
    filename_(filename),
    flags_(0)
{
    if (!s_soundmanager.existsDevice()) return;

    try
    {
        loadBufferFromFile(filename);
    } catch (Exception & e)
    {
        // Clear openal error state
        alGetError();
        alutGetError();
        throw;
    }
}


//------------------------------------------------------------------------------
SoundBuffer::~SoundBuffer()
{
    if (id_)
    {
        s_soundmanager.checkForErrors("before SoundBuffer::~SoundBuffer");
        alDeleteBuffers(1, &id_);
        s_soundmanager.checkForErrors("after SoundBuffer::~SoundBuffer");
    }
}



//------------------------------------------------------------------------------
/**
 *  Fills a sound buffer with data from a sound file.
 *
 *  \param new_sound_buffer The SoundBuffer struct that needs to be filled
 */
void SoundBuffer::loadBufferFromFile(const std::string & filename)
{
    s_soundmanager.checkForErrors("SoundBuffer::loadBufferFromFile");
    
    ALuint newBuffer;
    std::string total_filename = SOUND_PATH + filename;
    
    alGenBuffers(1, &newBuffer);
    if (alGetError() != AL_NO_ERROR)
    {
        Exception e("Error creating sound buffer for file ");
        e <<  total_filename << "!";
        throw(e);
    }

    id_ = newBuffer;

    ALvoid * data = NULL;
    
    // at first check if the file is an .ogg file
    if(total_filename.substr(total_filename.size()-3,3) == std::string("ogg"))
    {
        // XXXX due to an error inside the ogg lib, loading a non exsisting ogg file
        if(!existsFile(total_filename.c_str()))
        {
            s_log << Log::warning << " SoundBuffer: file not found: " << total_filename << "\n";
            return;
        }

        // try to load the ogg file
        if (!loadOgg(total_filename.c_str(), &data, (ALsizei*)&format_, &size_, &bits_, &freq_))
        {
            Exception e("Could not load OGG file ");
            e << total_filename.c_str() << " \n";
            throw e;            
        }

        // load data into buffer
        alBufferData(newBuffer, format_, data, size_, freq_);
        if (alGetError() != AL_NO_ERROR)
        {
            Exception e("Error filling sound buffer with data (");
            e << total_filename << ")!\n";
            throw e;
        }

        unloadOgg(format_, data, size_, freq_);

        s_log << Log::debug('r') << "Created sound buffer for ogg: " << total_filename << ".\n";
        return;
    }

    // now check whether sound file is supported file type from ALut
    id_ = alutCreateBufferFromFile((ALbyte*)total_filename.c_str());
    if(id_ != AL_NONE)
    {
        // file type supported and file loaded automatically into buffer
        // new OpenAL 1.1 funtctionality
    }
    else
    {        
        // print supported file types
        s_log << Log::debug('i')
              << "OpenAL supports following sound file MimeTypes: "
              << alutGetMIMETypes(ALUT_LOADER_BUFFER) << "\n";
        
        // file type not supported
        Exception e("Error loading sound file ");
        e << total_filename.c_str() << "\n";
        throw e;
    }
    
    s_log << Log::debug('r') << "Created sound buffer " << total_filename << ".\n";
}

const ALuint SoundBuffer::getId() const
{
    return id_;
}
