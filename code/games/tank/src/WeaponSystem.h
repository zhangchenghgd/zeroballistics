#ifndef TANKGAME_WEAPONSYSTEM_INCLUDED
#define TANKGAME_WEAPONSYSTEM_INCLUDED

#include <string>

#include "ParameterManager.h"
#include "Scheduler.h"



class Tank;
class PlayerInput;
class GameLogicServerCommon;
class GameLogicClientCommon;


namespace RakNet
{
    class BitStream;
}


#define s_weapon_system_loader Loki::SingletonHolder<dyn_class_loading::ClassLoader<WeaponSystem> , Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()

#define WEAPON_SYSTEM_IMPL(name) \
static WeaponSystem * create() { return new name(); } \
virtual std::string getType() const { return #name; }

//------------------------------------------------------------------------------
class WeaponSystem
{ 
 public:
    WEAPON_SYSTEM_IMPL(WeaponSystem);
    
    virtual ~WeaponSystem();

    Tank * getTank() const;

    bool handleInput(bool fire);


    virtual void init(Tank * tank, const std::string & section);
    
    virtual void frameMove(float dt);

    virtual float getCooldownStatus() const;
    void setCooldownStatus(float f);

    virtual void writeStateToBitstream (RakNet::BitStream & stream) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream);
    
    void setAmmo(unsigned ammo);
    unsigned getAmmo() const;

    float getDamage() const;

    const std::string & getSection() const;

    /// used as a workaround to be able to create projectiles, mines
    /// ...  directly inside the weapon system
    static void setGameLogicServer(GameLogicServerCommon * logic_server);  
    static void setGameLogicClient(GameLogicClientCommon * logic_client);  

    static bool weapon_system_debug_;    
    
 protected:

    WeaponSystem();

    virtual bool startFiring();
    virtual bool stopFiring();
    virtual void doFire()     {}
    virtual void onOverheat() {}

    virtual bool allowFiringAfterOverheat() const { return true; }

    void fire(float dt);
    bool canFire() const;


    bool isFiring()     const;
    bool isOverheated() const;
    bool isBlocked()    const;

    void checkForOverheating();
    void resetOverheated(void *);

    void blockFiring();
    void resetBlocked(void *);
    
    Tank * tank_;

    unsigned ammo_;
    float heating_;

    bool prev_firing_;

    std::string section_;


    mutable bool send_fire_; ///< This exists to transfer firing state
                             ///of instant-overheat weapons, such as
                             ///the main cannon.
    mutable bool send_overheated_;
    

    hTask task_fire_;
    hTask task_reset_overheated_;
    hTask task_reset_firing_blocked_; ///< After stopFiring,
                                      ///startFiring is blocked for
                                      ///fire_dt. Otherwise it can
                                      ///happen that the weapon is
                                      ///fired too quickly again after
                                      ///stopFiring/startFiring.
    
    RegisteredFpGroup fp_group_;

    static GameLogicServerCommon * game_logic_server_;
    static GameLogicClientCommon * game_logic_client_;
    
 private:
    // Don't allow copies
    WeaponSystem(const WeaponSystem&);
    WeaponSystem&operator=(const WeaponSystem&);

};

//------------------------------------------------------------------------------
class WeaponSystemServer : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(WeaponSystemServer);
};


#ifndef DEDICATED_SERVER
//------------------------------------------------------------------------------
class WeaponSystemClient : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(WeaponSystemClient);
};
#endif



#endif // #ifndef TANKGAME_WEAPONSYSTEM_INCLUDED
