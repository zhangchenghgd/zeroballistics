

#ifndef TANK_CONTROLLABLE_VISUAL_INCLUDED
#define TANK_CONTROLLABLE_VISUAL_INCLUDED


#include "RigidBodyVisual.h"

#include "Matrix.h"




namespace osgText
{
    class Text;
}

namespace osg
{
    class Geode;
}

//------------------------------------------------------------------------------
class ControllableVisual : public RigidBodyVisual
{
 public:
    virtual ~ControllableVisual();
    VISUAL_IMPL(ControllableVisual);

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);

    virtual Matrix getTrackingPos(const Vector & offset);
    
    void setLabelText(const std::string & text);
    void setLabelColor(const Color & color);
    
 protected:
    ControllableVisual();

    virtual void onModelChanged();

    osg::ref_ptr<osgText::Text> label_;
    osg::ref_ptr<osg::Geode> labelgeode_;

    bool prev_render_player_names_;
};





#endif
