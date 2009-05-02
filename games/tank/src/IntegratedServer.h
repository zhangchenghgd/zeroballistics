#ifndef TANK_INTEGRATED_SERVER_INCLUDED
#define TANK_INTEGRATED_SERVER_INCLUDED


#include "MetaTask.h"
#include "RegisteredFpGroup.h"


class NetworkServer;

//------------------------------------------------------------------------------
class IntegratedServer : public MetaTask
{
 public:
    IntegratedServer();
    virtual ~IntegratedServer();
    
    virtual void onFocus();

    void start();
    
    NetworkServer * getServer();
    
 protected:

    void onGameFinished();
    void onLevelLoaded();
    void reloadLevel(void*);
    
    std::auto_ptr<NetworkServer> server_;

    RegisteredFpGroup fp_group_;
};

#endif
