
#include "TextureManager.h"


#include <osgDB/ReadFile>

#include "SceneManager.h"

const unsigned MIN_QUALITY_TEXTURE_SIZE = 128;

//------------------------------------------------------------------------------
Texture::Texture(const std::string & filename)
{

    osg::ref_ptr<osg::Image> osg_image = osgDB::readImageFile(filename.c_str());
    if (!osg_image.get())
    {
        Exception e("Cannot load texture ");
        e << filename;
        throw e;
    }

    s_log << Log::debug('r')
          << filename
          << " has tex format ";
    if (osg_image->getPixelFormat() == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
    {
        s_log << "GL_COMPRESSED_RGBA_S3TC_DXT1_EXT";
    } else if (osg_image->getPixelFormat() == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
    {
        s_log << "GL_COMPRESSED_RGBA_S3TC_DXT3_EXT";
    }else if (osg_image->getPixelFormat() == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
    {
        s_log << "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT";
    }else if (osg_image->getPixelFormat() == GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
    {
        s_log << "GL_COMPRESSED_RGB_S3TC_DXT1_EXT";
    }else if (osg_image->getPixelFormat() == GL_BGRA)
    {
        s_log << "GL_BGRA";
    } else 
    {
        s_log << "UNKNOWN:"
              << osg_image->getPixelFormat();
    }
    s_log << "\n";

    osg_texture_ = new osg::Texture2D;
    osg_texture_->setImage(osg_image.get());

    
    unsigned texture_quality = s_params.get<unsigned>("client.graphics.texture_quality");
    if(texture_quality != 0) reduceSize(texture_quality);
    
    
    // bilinear.
    osg_texture_->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR_MIPMAP_LINEAR);

    // set anisotropic filtering between 1.0 and max
    float anisotropic_filtering = clamp((float)s_params.get<unsigned>("client.graphics.anisotropic_filtering"),
                                  1.0f, s_scene_manager.getMaxSupportedAnisotropy());
    osg_texture_->setMaxAnisotropy(anisotropic_filtering);
    

    osg_texture_->setWrap(osg::Texture2D::WRAP_S,osg::Texture2D::REPEAT);
    osg_texture_->setWrap(osg::Texture2D::WRAP_T,osg::Texture2D::REPEAT);
    osg_texture_->setWrap(osg::Texture2D::WRAP_R,osg::Texture2D::REPEAT);

    // Use image compression format
    osg_texture_->setInternalFormatMode(osg::Texture::USE_IMAGE_DATA_FORMAT);
    if (!osg_texture_->isCompressedInternalFormat())
    {
#ifdef ENABLE_DEV_FEATURES        
        s_log << Log::debug('r')
              << "Texture "
              << filename
              << " is uncompressed\n";
#endif
    }

    osg_texture_->setResizeNonPowerOfTwoHint(false);

    // Pre-caching
    osg_texture_->apply(s_scene_manager.getOsgState());


    s_texturemanager.total_texture_size_ += osg_image->getTotalSizeInBytesIncludingMipmaps();

    s_log << Log::debug('r')
          << "texture "
          << filename
          << " uses up "
          << (float)osg_image->getTotalSizeInBytesIncludingMipmaps() / (1024.0f * 1024.0f)
          << "MB. New total managed size: "
          << (float)s_texturemanager.total_texture_size_ / (1024.0f*1024.0f)
          << "\n";

}

//------------------------------------------------------------------------------
Texture::~Texture()
{
    assert(s_texturemanager.total_texture_size_ >= osg_texture_->getImage()->getTotalSizeInBytesIncludingMipmaps());
    s_texturemanager.total_texture_size_ -= osg_texture_->getImage()->getTotalSizeInBytesIncludingMipmaps();
}

//------------------------------------------------------------------------------
osg::Texture2D * Texture::getOsgTexture()
{
    return osg_texture_.get();
}

//------------------------------------------------------------------------------
void Texture::reduceSize(unsigned level)
{
    osg::Image * osg_image = osg_texture_->getImage();

    // Don't go below certain image size
    if (osg_image->getNumMipmapLevels() < 2        ||
        (unsigned)osg_image->s() <= MIN_QUALITY_TEXTURE_SIZE ||
        (unsigned)osg_image->t() <= MIN_QUALITY_TEXTURE_SIZE ) return;

    unsigned smaller_width  = osg_image->s();
    unsigned smaller_height = osg_image->t();


    // Must preserve aspect ratio
    for (unsigned l=0; l<level; ++l)
    {
        if ((smaller_width  >> 1) < MIN_QUALITY_TEXTURE_SIZE ||
            (smaller_height >> 1) < MIN_QUALITY_TEXTURE_SIZE)
        {
            level = l;
            break;
        }
        smaller_width  >>= 1;
        smaller_height >>= 1;
    }    

    // calculate the mem size of the mipmap lvl data
    unsigned int size = osg_image->computeRowWidthInBytes(smaller_width,
                                                          osg_image->getPixelFormat(),
                                                          osg_image->getDataType(),
                                                          osg_image->getPacking()) * smaller_height;

    // copy the smaller mipmap image data from original image and use it as
    // low res image for the texture (copy data* for use with NEW_DELETE allocation mode)
    unsigned char * mipmap_data = new unsigned char[size];
    memcpy(mipmap_data,osg_image->getMipmapData(level),size);


    osg::ref_ptr<osg::Image> osg_image_smaller = new osg::Image;
    osg_image_smaller->setImage(smaller_width,
                                smaller_height,
                                osg_image->r(),
                                osg_image->getInternalTextureFormat(),
                                osg_image->getPixelFormat(),
                                osg_image->getDataType(),
                                mipmap_data,
                                osg::Image::USE_NEW_DELETE);

    osg_image_smaller->setName(osg_image->getName());
        
    osg_texture_->setImage(osg_image_smaller.get());
}



//------------------------------------------------------------------------------
TextureManager::TextureManager() : 
    ResourceManager<Texture>("textures")
{
}
