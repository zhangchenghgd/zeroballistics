

#ifndef MODEL_VIEWER_INCLUDED
#define MODEL_VIEWER_INCLUDED

#include <osg/Node>
#include <osgDB/ReadFile>

#include "SceneManager.h"

#include "MetaTask.h"

#include "RegisteredFpGroup.h"
#include "Entity.h"
#include "OsgNodeWrapper.h"

class GUIConsole;
class NetworkServer;



const std::string VIEWER_CONFIG_FILE = "config_modelviewer.xml";

//------------------------------------------------------------------------------
class ModelViewer : public MetaTask
{
public:
    ModelViewer(); 
    virtual ~ModelViewer();

    virtual void render();
    
    virtual void onKeyDown(SDL_keysym sym);
    virtual void onKeyUp(SDL_keysym sym);
    virtual void onMouseButtonDown(const SDL_MouseButtonEvent & event);
    virtual void onMouseButtonUp(const SDL_MouseButtonEvent & event);
    virtual void onMouseMotion(const SDL_MouseMotionEvent & event);
    virtual void onResizeEvent(unsigned width, unsigned height);

    std::string loadModel(const std::vector<std::string> & args);
    std::vector<std::string> loadModelCompletionFun(const std::vector<std::string>&) const;    
    void reloadModel();
    
    std::string activateGroups(const std::vector<std::string> & args);
    std::string deactivateGroups(const std::vector<std::string> & args);
    std::vector<std::string> groupsCompletionFun(const std::vector<std::string>&) const;

    std::string setLodLevel(const std::vector<std::string> & args);
    void toggleLodUpdater() const;

    void saveCameraOrientation() const;
    void loadCameraOrientation();

    void setLightDirFromCamera() const;
    std::string setAmbient(const std::vector<std::string> & args);

 protected:

    // Input handling functions
    void up      (bool b);
    void down    (bool b);
    void forward (bool b);
    void backward(bool b);
    void right   (bool b);
    void left    (bool b);
    void speedup (bool b);
    void handleMouseMotion(Vector2d pos, Vector2d delta);

    void takeScreenshot();
    
    void registerInputHandlerFunctions();

    void loadParameterFiles();

    std::string setGroups(const std::vector<std::string> & args, bool enable);

    std::auto_ptr<GUIConsole> gui_console_;

    RegisteredFpGroup fp_group_;

    Entity camera_;

    osg::ref_ptr<OsgNodeWrapper> current_model_;
    std::string current_model_name_;
};



#endif
