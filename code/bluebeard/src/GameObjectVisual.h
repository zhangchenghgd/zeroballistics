

#ifndef TANK_GAME_OBJECT_VISUAL_INCLUDED
#define TANK_GAME_OBJECT_VISUAL_INCLUDED


#include <osg/NodeCallback>

#include "Singleton.h"
#include "RegisteredFpGroup.h"


class OsgNodeWrapper;
class GameObject;

#define s_visual_loader Loki::SingletonHolder<dyn_class_loading::ClassLoader<GameObjectVisual> , Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()

#define VISUAL_IMPL(name) \
static GameObjectVisual * create() { return new name(); } \
virtual std::string getType() const { return #name; }



//------------------------------------------------------------------------------
class GameObjectVisual : public osg::NodeCallback
{
 public:
    virtual ~GameObjectVisual();
    VISUAL_IMPL(GameObjectVisual);

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);

    
    void setGameObject(GameObject * object);

    OsgNodeWrapper * getWrapperNode();

    void setVisible(bool v);
    bool isVisible() const;


    void setModel(const std::string & model_name);
    
 protected:
    GameObjectVisual();

    virtual void onModelChanged();
    virtual void onGameObjectSet();
    void onGameObjectScheduledForDeletion();

    GameObject * object_;
    osg::ref_ptr<OsgNodeWrapper> osg_wrapper_;

    RegisteredFpGroup fp_group_;
};




#endif
