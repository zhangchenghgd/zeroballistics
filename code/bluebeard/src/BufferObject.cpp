
#include "BufferObject.h"

#include <osg/BufferObject>

#include "Utils.h"
#include "SceneManager.h"

osg::BufferObject::Extensions * BufferObject::extensions_ = NULL;

//------------------------------------------------------------------------------
/**
 *  \param buffer_type Must be one of GL_ELEMENT_ARRAY_BUFFER_ARB,
 *  GL_ARRAY_BUFFER_ARB, GL_PIXEL_PACK_BUFFER_ARB,
 *  GL_PIXEL_UNPACK_BUFFER_ARB.
*/
BufferObject::BufferObject(GLuint buffer_type) :
    name_(0),
    type_(buffer_type)
{
    if (!extensions_) extensions_ = osg::BufferObject::getExtensions(s_scene_manager.getContextId(),
                                                                     true);
    
    createBuffer();
}


//------------------------------------------------------------------------------
/**
 *  \param buffer_type Must be one of GL_ELEMENT_ARRAY_BUFFER_ARB,
 *  GL_ARRAY_BUFFER_ARB, GL_PIXEL_PACK_BUFFER_ARB,
 *  GL_PIXEL_UNPACK_BUFFER_ARB.
 *
 *  \param size The desired size of the buffer in bytes.
 *
 *  \param data Pointer to the buffer data. Can be NULL.
 *
 *  \param data_type One of the valid values for glBufferDataARB
 *  (e.g. STREAM_DRAW_ARB, STATIC_DRAW_ARB, DYNAMIC_DRAW_ARB)
 *
*/
BufferObject::BufferObject(GLuint buffer_type,
                           unsigned size, const void * data, GLuint data_type) :
    name_(0),
    type_(buffer_type)
{
    if (!extensions_) extensions_ = osg::BufferObject::getExtensions(s_scene_manager.getContextId(),
                                                                     true);
    
    createBuffer();
    setData(size, data, data_type);
}

//------------------------------------------------------------------------------
BufferObject::~BufferObject()
{
    extensions_->glDeleteBuffers(1, &name_);
}

//------------------------------------------------------------------------------
void BufferObject::setData(unsigned size, const void * data, GLuint data_type)
{
    assert(type_ == GL_ELEMENT_ARRAY_BUFFER_ARB ||
           type_ == GL_ARRAY_BUFFER_ARB         ||
           type_ == GL_PIXEL_PACK_BUFFER_ARB    ||
           type_ == GL_PIXEL_UNPACK_BUFFER_ARB);
    
    assert(data_type == GL_STREAM_DRAW_ARB  ||
           data_type == GL_STREAM_READ_ARB  ||
           data_type == GL_STREAM_COPY_ARB  ||
           data_type == GL_STATIC_DRAW_ARB  ||
           data_type == GL_STATIC_READ_ARB  ||
           data_type == GL_STATIC_COPY_ARB  ||
           data_type == GL_DYNAMIC_DRAW_ARB ||
           data_type == GL_DYNAMIC_READ_ARB ||
           data_type == GL_DYNAMIC_COPY_ARB);


    GLint prev_binding = getCurBinding();
    
    extensions_->glBindBuffer(type_, name_);
    extensions_->glBufferData(type_, size, data, data_type);
    extensions_->glBindBuffer(type_, prev_binding);
}

    
//------------------------------------------------------------------------------
void BufferObject::bind(GLuint type) const
{
    if (type == 0) type = type_;
    extensions_->glBindBuffer(type, name_);
}


//------------------------------------------------------------------------------
void BufferObject::unbind() const
{
    extensions_->glBindBuffer(type_, 0);
}

//------------------------------------------------------------------------------
void BufferObject::subData(unsigned offset, unsigned size, void* data) const
{
    GLint prev_binding = getCurBinding();
    
    extensions_->glBindBuffer(type_, name_);
    extensions_->glBufferSubData(type_, offset, size, data);
    extensions_->glBindBuffer(type_, prev_binding);
}

//------------------------------------------------------------------------------
/**
 *  Changes the current buffer binding.
 */
void * BufferObject::map(unsigned usage) const
{
    extensions_->glBindBuffer(type_, name_);
    void * ret = extensions_->glMapBuffer(type_, usage);

    return ret;
}

//------------------------------------------------------------------------------
/**
 *  Resets the buffer binding to zero.
 */
bool BufferObject::unmap() const
{
    extensions_->glBindBuffer(type_, name_);
    bool unmapped = extensions_->glUnmapBuffer(type_);
    assert(unmapped);
    extensions_->glBindBuffer(type_, 0);

    return unmapped;
}


//------------------------------------------------------------------------------
void BufferObject::createBuffer()
{
    extensions_->glGenBuffers(1, &name_);

    assert(name_);
}

//------------------------------------------------------------------------------
GLint BufferObject::getCurBinding() const
{
    GLint cur_binding = 0;
    switch (type_)
    {
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &cur_binding);
        break;
    case GL_ARRAY_BUFFER_ARB:
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &cur_binding);
        break;
    case GL_PIXEL_PACK_BUFFER_ARB:
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_ARB, &cur_binding);
        break;
    case GL_PIXEL_UNPACK_BUFFER_ARB:
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING_ARB, &cur_binding);
        break;
    default:
        assert(0);
    }

    return cur_binding;
}


