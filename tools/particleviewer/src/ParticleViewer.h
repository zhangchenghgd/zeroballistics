

#ifndef PARTICLE_VIEWER_INCLUDED
#define PARTICLE_VIEWER_INCLUDED


#include <osg/Node>
#include <osgDB/ReadFile>

#include "SceneManager.h"

#include "MetaTask.h"

#include "RegisteredFpGroup.h"
#include "Entity.h"
#include "OsgNodeWrapper.h"

class GUIConsole;
class NetworkServer;

const std::string VIEWER_CONFIG_FILE = "config_particleviewer.xml";

//------------------------------------------------------------------------------
class ParticleViewer : public MetaTask, public osg::Observer
{
public:
    ParticleViewer(); 
    virtual ~ParticleViewer();

    virtual void render();
    
    virtual void onKeyDown(SDL_keysym sym);
    virtual void onKeyUp(SDL_keysym sym);
    virtual void onMouseButtonDown(const SDL_MouseButtonEvent & event);
    virtual void onMouseButtonUp(const SDL_MouseButtonEvent & event);
    virtual void onMouseMotion(const SDL_MouseMotionEvent & event);
    virtual void onResizeEvent(unsigned width, unsigned height);

    virtual void objectDeleted(void * obj);
    
    std::vector<std::string> loadEffectsCompletionFun(const std::vector<std::string>& args) const;

    std::string addEffect(const std::vector<std::string> & args);
    std::string removeEffect(const std::vector<std::string> & args);
    void reloadEffects();
    void listEffectsLoaded();

    void saveCameraOrientation() const;
    void loadCameraOrientation();

    void toggleCoordAxes();

 protected:

    // Input handling functions
    void up     (bool b);
    void down   (bool b);
    void forward (bool b);
    void backward(bool b);
    void right  (bool b);
    void left   (bool b);
    void handleMouseMotion(Vector2d pos, Vector2d delta);
    void takeScreenshot();

    void restartEffect(void* eff);
    
    void loadEffect(const std::string & name);

    void registerInputHandlerFunctions();

    void loadParameterFiles();

    std::auto_ptr<GUIConsole> gui_console_;

    RegisteredFpGroup fp_group_;

    Entity camera_;

    std::vector<std::string> effects_list_;
    
    std::vector<osg::Group*> effects_; ///< No ref_ptr as we get notified
                                 ///via observer mechanism if an
                                 ///effect is deleted and restart it
                                 ///immediately afterwards.

    osg::ref_ptr<osg::Geode> coord_axes_;
};



#endif
