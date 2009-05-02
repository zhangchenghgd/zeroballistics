

#ifndef TANK_GAMEOBJECT_INCLUDED
#define TANK_GAMEOBJECT_INCLUDED


#include <string>

#include <raknet/RakNetTypes.h>

#include "Matrix.h"
#include "Vector.h"
#include "Observable.h"


class GameState;

namespace RakNet
{
    class BitStream;
}



//------------------------------------------------------------------------------
/**
 *  Used to specify what portion of the object state to read/write.
 */
enum OBJECT_STATE_TYPE
{
    OST_CORE  = 1, ///< Predicted state, rigidbody state, states which
                   ///are expected to change every frame. This alone
                   ///gets sent unreliably over the network.
    OST_EXTRA = 2, ///< Reload time, amunition etc. States which are
                   ///expected to change infrequently. Gets sent
                   ///reliably.
    OST_BOTH  = 3, ///< Both of the above.
    OST_CLIENT_SIDE_PREDICTION = 4
};


//------------------------------------------------------------------------------
/**
 *  Events for game object observers.
 */
enum GAME_OBJECT_EVENT
{
    GOE_SCHEDULED_FOR_DELETION,  ///< Called when the object is scheduled for deletion.
    GOE_LAST
};


#define s_game_object_loader Loki::SingletonHolder<dyn_class_loading::ClassLoader<GameObject> , Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()


#define GAME_OBJECT_IMPL(name) \
static GameObject * create() { return new name(); } \
virtual std::string getType() const { return #name; }


const uint16_t INVALID_GAMEOBJECT_ID = (uint16_t)-1;



//------------------------------------------------------------------------------
/**
 *  Represents an object in the game. Optionally can be rendered,
 *  optionally is attached to a rigid body, optionally can collide
 *  with other bodies.
 */
class GameObject : public Observable
{
 public:
    virtual ~GameObject();

    GAME_OBJECT_IMPL(GameObject);


    const std::string & getName() const; 
    
    const SystemAddress & getOwner() const;
    void setOwner(const SystemAddress & id);

    void setId(uint16_t id);
    uint16_t getId() const;
    
    virtual void frameMove(float dt);

    // used for object creation
    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp);

    virtual void writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const               {}
    virtual void readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp) {}

    bool isScheduledForDeletion() const;
    virtual void scheduleForDeletion();

    bool isDirty() const;
    void clearDirty();


    void * getUserData() const;
    void setUserData(void * data);


    static GameObject * create(const std::string & name,
                               const std::string & type,
                               bool create_visual);
    
 protected:

    GameObject();        

    uint16_t id_;         ///< The id this object is known as in
                          ///GameState and across the network.
    
    SystemAddress owner_; ///< For Controllable objects this is the
                          ///player controlling the object (or
                          ///UNASSIGNED_SYSTEM_ADDRESS)

    std::string name_;

    bool scheduled_for_deletion_;

    void * user_data_; ///< On client: points to an instance of GameObjectVisual
    
    mutable bool network_state_dirty_; ///< If true, send extra state over the network

    // Don't allow direct copies
    GameObject(const GameObject&);
    GameObject&operator=(const GameObject&);
};



//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const GameObject & object);



#endif
