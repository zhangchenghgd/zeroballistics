#ifndef TANKGAME_WEAPONSYSTEM_INCLUDED
#define TANKGAME_WEAPONSYSTEM_INCLUDED

#include <string>

#include "ParameterManager.h"

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

    void reset(Tank * tank, const std::string & section);
    Tank * getTank() const;

    virtual void frameMove(float dt);

    virtual bool fire(bool fire) { return false; };  ///< handles input from controllable

    virtual void writeStateToBitstream (RakNet::BitStream & stream) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream);
    
    void setAmmo(unsigned ammo);
    unsigned getAmmo() const;

    float getDamage() const;
    
    float getReloadPercentage() const;

    /// used as a workaround to be able to create projectiles, mines ...  directly inside the weapon system
    static void setGameLogicServer(GameLogicServerCommon * logic_server);  
    static void setGameLogicClient(GameLogicClientCommon * logic_client);  
    
 protected:

    WeaponSystem();

    Tank * tank_;

    float reload_time_; ///< Zero means weapon is fully reloaded.
    unsigned ammo_;

    static GameLogicServerCommon * game_logic_server_;
    static GameLogicClientCommon * game_logic_client_;

    std::string section_;
    
 private:
    // Don't allow copies
    WeaponSystem(const WeaponSystem&);
    WeaponSystem&operator=(const WeaponSystem&);

};

#endif
