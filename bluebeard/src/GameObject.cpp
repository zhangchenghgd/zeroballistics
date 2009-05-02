
#include "GameObject.h"



#include <raknet/BitStream.h>

#include "NetworkCommand.h"
#include "PuppetMasterClient.h"
#include "ParameterManager.h"
#include "ClassLoader.h"
#include "Paths.h"

#ifndef DEDICATED_SERVER
#include "GameObjectVisual.h"
#endif


// For reliable registration...
#include "RigidBody.h"
#include "Water.h"


struct GameObjectRegistrator
{
    GameObjectRegistrator()
        {
            using namespace dyn_class_loading;
            ClassLoader<GameObject> & loader = Loki::SingletonHolder<ClassLoader<GameObject> , Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance();

            loader.addRegisteredClass("GameObject", &GameObject::create);
            loader.addRegisteredClass("RigidBody",  &RigidBody::create);
            loader.addRegisteredClass("Water",      &Water::create);
        }
} go_registrator;




//------------------------------------------------------------------------------
GameObject::~GameObject()
{
    scheduleForDeletion();
}


//------------------------------------------------------------------------------
const std::string & GameObject::getName() const
{
    return name_;
}


//------------------------------------------------------------------------------
const SystemAddress & GameObject::getOwner() const
{
    return owner_;
}

//------------------------------------------------------------------------------
void GameObject::setOwner(const SystemAddress & id)
{
    owner_ = id;
}


//------------------------------------------------------------------------------
void GameObject::setId(uint16_t id)
{
    id_ = id;
}


//------------------------------------------------------------------------------
uint16_t GameObject::getId() const
{
    return id_;
}


//------------------------------------------------------------------------------
void GameObject::frameMove(float dt)
{
}

//------------------------------------------------------------------------------
void GameObject::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    stream.Write(owner_);
    network::writeToBitstream(stream, name_);
}

//------------------------------------------------------------------------------
void GameObject::readInitValuesFromBitstream(RakNet::BitStream & stream,
                                             GameState * game_state_)
{
    stream.Read(owner_);
    network::readFromBitstream(stream, name_);
    
#ifndef DEDICATED_SERVER    
    GameObjectVisual * new_visual = s_visual_loader.create(getType() + "Visual");    
    new_visual->setGameObject(this);
#endif
}

//------------------------------------------------------------------------------
bool GameObject::isScheduledForDeletion() const
{
    return scheduled_for_deletion_;
}

//------------------------------------------------------------------------------
/**
 *  We cannot delete objects from within a collision callback because
 *  collision events could still be pending for this
 *  object. Therefore, this function is needed.
 */
void GameObject::scheduleForDeletion()
{
    s_log << Log::debug('l')
          << "Scheduled object "
          << *this
          << " for deletion\n";

    if (!scheduled_for_deletion_)
    {
        scheduled_for_deletion_ = true;
        emit(GOE_SCHEDULED_FOR_DELETION);
    }
}

//------------------------------------------------------------------------------
bool GameObject::isDirty() const
{
    return network_state_dirty_;
}

//------------------------------------------------------------------------------
void GameObject::clearDirty()
{
    network_state_dirty_ = false;
}

//------------------------------------------------------------------------------
/**
 *  Currently returns the visual belonging to this object.
 */
void * GameObject::getUserData() const
{
    return user_data_;
}

//------------------------------------------------------------------------------
void GameObject::setUserData(void * data)
{
    user_data_ = data;
}

//------------------------------------------------------------------------------
/**
 *  Creates a GameObject of the specified type, and additionally, if
 *  DEDICATED_SERVER is not defined, a new Visual corresponding to the
 *  type.
 */
GameObject * GameObject::create(const std::string & name,
                                const std::string & type,
                                bool create_visual)
{
    assert(!name.empty());

    GameObject * new_object = s_game_object_loader.create(type);
    new_object->name_ = name;
    
#ifndef DEDICATED_SERVER
    if (create_visual)
    {
        GameObjectVisual * new_visual = s_visual_loader.create(type + "Visual");    
        new_visual->setGameObject(new_object);
    }
#endif    


    return new_object;
}



//------------------------------------------------------------------------------
GameObject::GameObject() :
    id_(INVALID_GAMEOBJECT_ID),
    owner_(UNASSIGNED_SYSTEM_ADDRESS),
    scheduled_for_deletion_(false),
    user_data_(NULL),
    network_state_dirty_(true)
{
}

//------------------------------------------------------------------------------
GameObject::GameObject(const GameObject & other) :
    id_(INVALID_GAMEOBJECT_ID),
    owner_(UNASSIGNED_SYSTEM_ADDRESS),
    name_(other.name_),
    scheduled_for_deletion_(false),
    user_data_(NULL),
    network_state_dirty_(true)
{
}


//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const GameObject & object)
{
    out << object.getName()
        << " (" << (unsigned)object.getId() << ")";
    return out;
}
