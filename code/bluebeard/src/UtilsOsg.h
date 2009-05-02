
#ifndef LIB_UTILSOSG_INCLUDED
#define LIB_UTILSOSG_INCLUDED

#include <osg/Matrix>
#include <osg/Geode>
#include <osg/Drawable>

#include "Matrix.h"




#define CHECK_FOR_GL_ERROR \
{ \
GLenum error = glGetError(); \
if (error != GL_NO_ERROR) { \
std::cout << "GL error in " << __FILE__ << " line " << __LINE__ \
<< ": " << gluErrorString(error) << "\n"; } }



namespace osg
{
    class Camera;
}

//------------------------------------------------------------------------------
/**
 *  Used to handle osg double/single precision.
 *
 *  Converts a pointer to 16 osg double to a pointer to 16 opengl floats.
 */
inline const Matrix matOsg2Gl(const osg::Matrix & p)
{
    return Matrix(p.ptr());
}

//------------------------------------------------------------------------------
inline const osg::Matrix matGl2Osg(const float * p)
{
    return osg::Matrix(p);
}


//------------------------------------------------------------------------------
inline osg::Vec3 vecGl2Osg(const Vector & v)
{
    return osg::Vec3(v.x_, v.y_, v.z_);
}

//------------------------------------------------------------------------------
inline Vector vecOsg2Gl(const osg::Vec3 & v)
{
    return Vector(v.x(), v.y(), v.z());
}



//------------------------------------------------------------------------------
/**
 *  Renders the view frustum for the specified camera by transforming
 *  the two-unit cube with its inverse projection and view matrix.
 */
class CameraGeode : public osg::Geode
{
 public:
    CameraGeode(osg::Camera * camera);    

 protected:

    virtual ~CameraGeode();

    class CameraDrawable : public osg::Drawable
        {
        public:
            CameraDrawable(osg::Camera * camera);

            virtual void drawImplementation(osg::RenderInfo& renderInfo) const;
            virtual osg::Object* cloneType() const;
            virtual osg::Object* clone(const osg::CopyOp& copyop) const;

        protected:
            CameraDrawable();
            virtual ~CameraDrawable();
            osg::ref_ptr<osg::Camera> camera_node_; ///< The
                                                        ///camera node
                                                        ///to render.
            
        };
};

void saveScreenshot(const std::string & name);

#endif // ifndef LIB_UTILSOSG_INCLUDED
