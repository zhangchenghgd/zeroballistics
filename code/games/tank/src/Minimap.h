
#ifndef TANK_MINIMAP_INCLUDED
#define TANK_MINIMAP_INCLUDED


#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osgUtil/CullVisitor>



#include "Log.h"
#include "Matrix.h"


#include "RegisteredFpGroup.h"
#include "Vector2d.h"

namespace osg
{
    class TexMat;
    class Projection;
    class MatrixTransform;
}

namespace bbm
{
    class LevelData;
}

class PuppetMasterClient;
class Observable;
class RigidBody;
class HudTextureElement;

//------------------------------------------------------------------------------
struct MinimapIcon
{
    MinimapIcon(osg::MatrixTransform * transform) :
        transform_(transform) {}

    osg::ref_ptr<osg::MatrixTransform> transform_;
    RegisteredFpGroup fp_group_; ///< Used for flashing the icon
};


//------------------------------------------------------------------------------
class Minimap
{
 public:

    typedef std::map<RigidBody*, MinimapIcon > BodyIconContainer;
    typedef std::vector<std::pair<Matrix, osg::ref_ptr<osg::MatrixTransform> > > StaticIconContainer;
    typedef std::vector<osg::ref_ptr<osg::MatrixTransform> > NodeContainer;
    
    Minimap(PuppetMasterClient * master);
    virtual ~Minimap();

    void setupBackground(const std::string file, float level_size);

    void update();

    void clear();

    void addNode    (osg::Node * node);

    void addIcon    (RigidBody * body,      const std::string & icon);
    void addIcon    (const Matrix & offset, const std::string & icon);

    bool hasIcon(RigidBody * body) const;
    
    bool removeIcon (RigidBody * body);
    void enableFlash(RigidBody * body, bool b);
    void enableFlash(RigidBody * body, float duration);

    void onWindowResized();

    void enable(bool e);
    
 protected:

    void onBodyScheduledForDeletion(Observable * body, unsigned event);

    void flashIcon(void * icon);
    void stopFlashingIcon(void * i);

    osg::Matrix getIconTransform(const Vector & viewpos, const Matrix & icon_pos,
                                 float map_extents, float icon_size) const;
    
    PuppetMasterClient * master_;
    
    Vector2d map_center_; ///< Position of the minimap texture center in the world
    float size_;          ///< Width and height of the area covered by
                          ///the minimap texture
    
    osg::ref_ptr<osg::Projection>   proj_;
    osg::ref_ptr<osg::Group>        icon_group_; ///< Master group containing all minimap stuff
    osg::ref_ptr<osg::Group>        bg_group_;   ///< Master group containing background image and border
    
    osg::ref_ptr<HudTextureElement> map_texture_; ///< The texture containing the actual map
    osg::ref_ptr<osg::TexMat>       tex_matrix_;  ///< Used to display the proper part of map_texture_


    BodyIconContainer icon_;
    StaticIconContainer static_icon_;
    
    NodeContainer node_;

    RegisteredFpGroup fp_group_;
};


//------------------------------------------------------------------------------
class MinimapCircle : public osg::Geometry
{
 public:
     MinimapCircle(float pos_x, float pos_y, float radius) :
       pos_x_(pos_x),
       pos_y_(pos_y),
       radius_(radius) 
       {
           init();
       }

    void init()
       {
            const unsigned sections = 40;
            const unsigned num_vertices = sections + 2;
            const float two_pi = 2.0f * osg::PI;

            // Create buffers
            addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLE_FAN, 0, num_vertices));

            osg::Vec3Array * vertex = new osg::Vec3Array(num_vertices); // vec3 or computeBounds will complain
            setVertexArray(vertex);
                
            osg::Vec4Array * osg_color = new osg::Vec4Array(1);
            setColorArray(osg_color);
            setColorBinding(osg::Geometry::BIND_OVERALL);
            (*osg_color)[0].set(0.0f, 0.0f, 0.0f, 1.0f);   // set color

            // draw circle
            (*vertex)[0].set(pos_x_, pos_y_, 0.0f); // origin
            for(unsigned i=0; i <= sections; i++) // make sections number of slices
            { 
                   (*vertex)[i+1].set(  pos_x_ + (radius_ * cos(i *  two_pi / sections)), 
                                        pos_y_ + (radius_* sin(i * two_pi / sections)),
                                        0.0f);
            }
       }

    virtual ~MinimapCircle() {};

    virtual const char * className () const { return "MinimapCircle"; }

    float pos_x_;
    float pos_y_;
    float radius_;
};

#endif
