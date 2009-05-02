
#ifndef RACING_BUFFER_OBJECT_INCLUDED
#define RACING_BUFFER_OBJECT_INCLUDED


#define BUFFER_OFFSET(i) ((char *)NULL + (i))


#include <osg/BufferObject>

//------------------------------------------------------------------------------
class BufferObject
{
 public:
    BufferObject(GLuint buffer_type);
    BufferObject(GLuint buffer_type,
                 unsigned size, const void * data, GLuint data_type);
    virtual ~BufferObject();

    void setData(unsigned size, const void * data, GLuint data_type);
    
    void bind(GLuint type = 0) const;
    void unbind() const;

    void subData(unsigned offset, unsigned size, void* data) const;


    void * map(unsigned usage = GL_WRITE_ONLY_ARB) const;
    bool unmap() const;
    
 protected:

    void createBuffer();

    GLint getCurBinding() const;

    static osg::BufferObject::Extensions * extensions_;
    
    GLuint name_; ///< The GL name of the buffer.
    GLuint type_; ///< Must be one of GL_ELEMENT_ARRAY_BUFFER_ARB,
                  ///GL_ARRAY_BUFFER_ARB, GL_PIXEL_PACK_BUFFER_ARB,
                  ///GL_PIXEL_UNPACK_BUFFER_ARB.
    
};


#endif // #ifndef RACING_BUFFER_OBJECT_INCLUDED
