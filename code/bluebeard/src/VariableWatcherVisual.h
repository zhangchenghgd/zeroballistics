

#ifndef BLUEBEARD_VARIABLE_WATCHER_VISUAL_INCLUDED
#define BLUEBEARD_VARIABLE_WATCHER_VISUAL_INCLUDED


#include <osg/Drawable>

class GraphedVar;
class VariableWatcherVisualUpdateCallback;

namespace osg
{
    class Geode;
}

namespace osgText
{
    class Text;
}

//------------------------------------------------------------------------------
class VariableWatcherVisual : public osg::Drawable
{
    friend class VariableWatcherVisualUpdateCallback;
 public:
    VariableWatcherVisual();
    
 protected:
    // Osg overrides
    virtual const char * className () const { return "VariableWatcherVisual"; }
    virtual void drawImplementation(osg::RenderInfo& renderInfo) const;
    virtual osg::Object* cloneType() const;
    virtual osg::Object* clone(const osg::CopyOp& copyop) const;

    void update();
    
    void renderGraphedVar(const GraphedVar * var, float x, float y) const;

    osg::Geode * geode_; ///< Geode containing this drawable. Avoid cyclic ref_ptr reference -> plain pointer only.

    std::vector<osg::ref_ptr<osgText::Text> > label_;
};

#endif
