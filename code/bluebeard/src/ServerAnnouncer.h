

#ifndef TANKS_SERVER_ANNOUNCER_INCLUDED
#define TANKS_SERVER_ANNOUNCER_INCLUDED


#include <vector>
#include <map>

#include "RegisteredFpGroup.h"

struct SystemAddress;
class Observable;
class PuppetMasterServer;

//------------------------------------------------------------------------------
/**
 *  Reads some predefined messages to any player that newly connects.
 */
class ServerAnnouncer
{
 public:
    ServerAnnouncer(PuppetMasterServer * puppet_master);
 protected:

    void loadMessages();
    
    void onPlayerJoined(Observable*, void * address, unsigned);

    void sendMessage(void*);
    
    RegisteredFpGroup fp_group_;


    std::vector<std::pair<std::string, unsigned> > message_; ///< Message + delay after message
    std::map<SystemAddress, unsigned> message_index_; ///< for each player, store which message to transmit next.

    PuppetMasterServer * puppet_master_;
};

#endif
