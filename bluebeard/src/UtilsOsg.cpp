
#include "UtilsOsg.h"


#ifdef ENABLE_DEV_FEATURES
#include <GL/glut.h>
#endif


#include <osg/Camera>
#include <osg/LineWidth>
#include <osg/Image>
#include <osgDB/WriteFile>


#include "Log.h"


//------------------------------------------------------------------------------
CameraGeode::CameraGeode(osg::Camera * camera)
{
    setName("CameraGeode");
    
    osg::Drawable * drawable = new CameraDrawable(camera);
    drawable->setUseDisplayList(false);
    
    osg::ref_ptr<osg::StateSet> state_set = drawable->getOrCreateStateSet();
    state_set->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    state_set->setAttribute(new osg::LineWidth(2));

    addDrawable(drawable);
}

//------------------------------------------------------------------------------
CameraGeode::~CameraGeode()
{
}


//------------------------------------------------------------------------------
CameraGeode::CameraDrawable::CameraDrawable(osg::Camera * camera) :
    camera_node_(camera)
{
}


//------------------------------------------------------------------------------
void CameraGeode::CameraDrawable::drawImplementation(osg::RenderInfo& renderInfo) const
{
    // ---------- Render eye with coordinate system ----------
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    osg::Matrix osg_mat = camera_node_->getInverseViewMatrix();
    Matrix gl_mat = matOsg2Gl(osg_mat);
    
    glMultMatrixf((GLfloat*)&gl_mat._11);


    glColor3f(0,0,1);
#ifdef ENABLE_DEV_FEATURES    
    glutSolidSphere(0.05, 10, 10);
#endif    

    glBegin(GL_LINES);
    glColor3f(1,0,1); // Magenta: viewing dir
    glVertex3f(0,0,0);
    glVertex3f(0,0,-1);

    glColor3f(0,1,0); // Green: up dir
    glVertex3f(0,0,0);
    glVertex3f(0,1,0);

    glColor3f(1,0,0); // Blue: right dir
    glVertex3f(0,0,0);
    glVertex3f(1,0,0);

    glEnd();

    glPopMatrix();



    // ---------- Render viewing frustum ----------
    osg::Matrixf inv = camera_node_->getProjectionMatrix();
    inv.invert(inv);

    inv = inv * camera_node_->getInverseViewMatrix();

    glPushMatrix();

    glMultMatrixf((GLfloat*)inv.ptr());




    
    osg::Vec4f corner[8] = {osg::Vec4d( 1, 1,-1,1),
                            osg::Vec4d( 1,-1,-1,1),
                            osg::Vec4d(-1,-1,-1,1),
                            osg::Vec4d(-1, 1,-1,1),
                            osg::Vec4d( 1, 1, 1,1),
                            osg::Vec4d( 1,-1, 1,1),
                            osg::Vec4d(-1,-1, 1,1),
                            osg::Vec4d(-1, 1, 1,1)};



    glColor3f(0,1,0); // green: near plane
    
    glBegin(GL_LINE_LOOP);
    glVertex3fv((GLfloat*)&corner[0]._v);
    glVertex3fv((GLfloat*)&corner[1]._v);
    glVertex3fv((GLfloat*)&corner[2]._v);
    glVertex3fv((GLfloat*)&corner[3]._v);
    glEnd();



    glColor3f(0,0,1); // blue: far plane
    
    glBegin(GL_LINE_LOOP);
    glVertex3fv((GLfloat*)&corner[4]._v);
    glVertex3fv((GLfloat*)&corner[5]._v);
    glVertex3fv((GLfloat*)&corner[6]._v);
    glVertex3fv((GLfloat*)&corner[7]._v);
    glEnd();

    glColor3f(1,0,0); // red:sides
    
    glBegin(GL_LINES);
    glVertex3fv((GLfloat*)&corner[0]._v);
    glVertex3fv((GLfloat*)&corner[4]._v);
    
    glVertex3fv((GLfloat*)&corner[1]._v);
    glVertex3fv((GLfloat*)&corner[5]._v);
    
    glVertex3fv((GLfloat*)&corner[2]._v);
    glVertex3fv((GLfloat*)&corner[6]._v);
    
    glVertex3fv((GLfloat*)&corner[3]._v);
    glVertex3fv((GLfloat*)&corner[7]._v);
    glEnd();

    glPopMatrix();

}


//------------------------------------------------------------------------------
osg::Object* CameraGeode::CameraDrawable::cloneType() const
{
    return new CameraDrawable();
}


//------------------------------------------------------------------------------
osg::Object* CameraGeode::CameraDrawable::clone(const osg::CopyOp& copyop) const
{
    return new CameraDrawable(camera_node_.get());
}


//------------------------------------------------------------------------------
CameraGeode::CameraDrawable::CameraDrawable()
{
}

//------------------------------------------------------------------------------
CameraGeode::CameraDrawable::~CameraDrawable()
{
}


//------------------------------------------------------------------------------
void saveScreenshot(const std::string & name)
{
    static std::string last_name;
    static unsigned index=0;
    if (name != last_name) index=0;
    last_name = name;
    
    static osg::ref_ptr<osg::Image> shot = new osg::Image;
    
    GLint dims[4];
    glGetIntegerv(GL_VIEWPORT, dims);

    shot->readPixels(0,0,dims[2], dims[3], GL_RGB, GL_UNSIGNED_BYTE);

    std::string full_path;
    do
    {
        std::ostringstream strstr;
        strstr << std::setfill('0')
                  << name
                  << std::setw(6)
                  << index
                  << ".jpg";
        ++index;
        full_path = strstr.str();
    } while(existsFile(full_path.c_str()));
    
    bool ret = osgDB::writeImageFile(*shot.get(), full_path);

    if (!ret)
    {
        s_log << Log::error
              << "Error saving "
              << full_path
              << "\n";
    }
}

