
#include "ServerAnnouncer.h"


#include "raknet/RakNetTypes.h"

#include "PuppetMasterServer.h"
#include "Log.h"
#include "ParameterManager.h"
#include "Scheduler.h"


//------------------------------------------------------------------------------
ServerAnnouncer::ServerAnnouncer(PuppetMasterServer * puppet_master) :
    puppet_master_(puppet_master)
{
    puppet_master->addObserver(ObserverCallbackFunUserData(this, &ServerAnnouncer::onPlayerJoined),
                               PMOE_PLAYER_JOINED,
                               &fp_group_);

    try
    {
        loadMessages();
    } catch (Exception & e)
    {
        s_log << "Announcer disabled: "
              << e
              << "\n";
    }
}


//------------------------------------------------------------------------------
void ServerAnnouncer::loadMessages()
{
    std::vector<std::string> msgs = s_params.get<std::vector<std::string> >("server.announcer.messages");

    if (msgs.size() & 1) throw Exception("Bad message format");

    for (unsigned i=0; i<(msgs.size()>>1); ++i)
    {
        message_.push_back(std::make_pair(msgs[2*i], fromString<unsigned>(msgs[2*i+1])));
    }
}


//------------------------------------------------------------------------------
void ServerAnnouncer::onPlayerJoined(Observable*, void * a, unsigned)
{
    if (message_.empty()) return;
    
    SystemAddress & address = *((SystemAddress*)a);

    message_index_[address] = 0;

    sendMessage(new SystemAddress(address));
}


//------------------------------------------------------------------------------
void ServerAnnouncer::sendMessage(void*a)
{
    std::auto_ptr<SystemAddress> address((SystemAddress*)a);

    std::map<SystemAddress, unsigned>::iterator it = message_index_.find(*address);
    
    if (it == message_index_.end())           return;
    if (!puppet_master_->getPlayer(*address)) return;

    std::string msg = message_[it->second].first;
    unsigned delay  = message_[it->second].second;

    // Loop messages
    if (++it->second == message_.size()) it->second = 0;

    puppet_master_->sendServerMessage(msg, *address);

    s_scheduler.addEvent(SingleEventCallback(this, &ServerAnnouncer::sendMessage),
                         delay,
                         address.release(),
                         "ServerAnnouncer::sendMessage",
                         &fp_group_);
}
