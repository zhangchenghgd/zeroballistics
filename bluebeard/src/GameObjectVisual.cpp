


#include "GameObjectVisual.h"


#include "GameObject.h"
#include "SceneManager.h"
#include "ClassLoader.h"
#include "ReaderWriterBbm.h"
#include "OsgNodeWrapper.h"


// For reliable registration...
#include "RigidBodyVisual.h"
#include "ControllableVisual.h"
#include "WaterVisual.h"


struct GameObjectVisualRegistrator
{
    GameObjectVisualRegistrator()
        {
            using namespace dyn_class_loading;
            ClassLoader<GameObjectVisual> & loader = Loki::SingletonHolder<ClassLoader<GameObjectVisual> , Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance();

            loader.addRegisteredClass("GameObjectVisual",   &GameObjectVisual::create);
            loader.addRegisteredClass("RigidBodyVisual",    &RigidBodyVisual::create);
            loader.addRegisteredClass("ControllableVisual", &ControllableVisual::create);
            loader.addRegisteredClass("WaterVisual",        &WaterVisual::create);
        }
} gov_registrator;



//------------------------------------------------------------------------------
GameObjectVisual::~GameObjectVisual()
{
    assert(!osg_wrapper_);
    assert(!object_);
}


//------------------------------------------------------------------------------
void GameObjectVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{
    traverse(node, nv);    
}


//------------------------------------------------------------------------------
void GameObjectVisual::setGameObject(GameObject * object)
{
    object_ = object;
    object_->setUserData(this);

    onGameObjectSet();


    // Do this last to make sure any other cleanup observers are
    // registered first (e.g. for instanced geometry)
    object_->addObserver(ObserverCallbackFun0(this, &GameObjectVisual::onGameObjectScheduledForDeletion),
                         GOE_SCHEDULED_FOR_DELETION, &fp_group_);


    // Our true owner, the game object set in this function, cannot
    // hold a refptr -> we have to manually keep track of this
    // reference.
    ref();
}


//------------------------------------------------------------------------------
OsgNodeWrapper * GameObjectVisual::getWrapperNode()
{
    return osg_wrapper_.get();
}


//------------------------------------------------------------------------------
void GameObjectVisual::setVisible(bool v)
{
    osg_wrapper_->getOsgNode()->setNodeMask(v ? NODE_MASK_VISIBLE : NODE_MASK_INVISIBLE);
}

//------------------------------------------------------------------------------
bool GameObjectVisual::isVisible() const
{
    return osg_wrapper_->getOsgNode()->getNodeMask() == NODE_MASK_VISIBLE;
}



//------------------------------------------------------------------------------
void GameObjectVisual::setModel(const std::string & model_name)
{
    if (osg_wrapper_.get())
    {
        osg_wrapper_->removeFromScene();
        osg_wrapper_ = NULL;
    }

    try
    {
        osg_wrapper_ = ReaderWriterBbm::loadModel(model_name);
    } catch (Exception & e)
    {
        e.addHistory("GameObjectVisual::setModel(" + model_name + ")");
        throw e;
    }

    osg_wrapper_->addToScene();
    onModelChanged();
}



//------------------------------------------------------------------------------
GameObjectVisual::GameObjectVisual() :
    object_(NULL),
    osg_wrapper_(NULL)
{
}


//------------------------------------------------------------------------------
void GameObjectVisual::onModelChanged()
{
}



//------------------------------------------------------------------------------
void GameObjectVisual::onGameObjectSet()
{
}



//------------------------------------------------------------------------------
/**
 *  Gets called when the object this visual is responsible for gets
 *  deleted. Since osg_wrapper_ is the only user of this object, it
 *  should be deleted as well.
 */
void GameObjectVisual::onGameObjectScheduledForDeletion()
{
    s_log << Log::debug('d')
          << "GameObjectVisual::onGameObjectScheduledForDeletion for "
          << object_->getId()
          << "\n";
    
    object_->setUserData(NULL);
    object_   = NULL;

    if (osg_wrapper_.get())
    {
        osg_wrapper_->removeFromScene();    
        osg_wrapper_ = NULL;
    }
    
    // see GameObjectVisual::setGameObject()
    assert(referenceCount() == 1); // make sure our own reference is the last one.
    unref(); // commit suicide.
}


