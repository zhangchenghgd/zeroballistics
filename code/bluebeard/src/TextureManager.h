
#ifndef BLUEBEARD_TEXTUREMANAGER_INCLUDED
#define BLUEBEARD_TEXTUREMANAGER_INCLUDED

#include "ResourceManager.h"

#include <osg/Texture2D>

//------------------------------------------------------------------------------
class Texture
{
 public:
    Texture(const std::string & filename);
    virtual ~Texture();

    osg::Texture2D * getOsgTexture();

    void reduceSize(unsigned level);
    
 private:
    osg::ref_ptr<osg::Texture2D> osg_texture_;

};




#define s_texturemanager Loki::SingletonHolder<TextureManager, Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()
//------------------------------------------------------------------------------
class TextureManager : public ResourceManager<Texture>
{
    DECLARE_SINGLETON(TextureManager);

    friend class Texture;
    
 public:
     virtual ~TextureManager() {};

     
 protected:
     
     unsigned total_texture_size_; ///< Total size of all managed textures in bytes.
};

#endif // #ifndef BLUEBEARD_TEXTUREMANAGER_INCLUDED
