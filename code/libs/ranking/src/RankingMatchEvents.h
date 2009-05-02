
#ifndef RANKING_MATCH_EVENTS_INCLUDED
#define RANKING_MATCH_EVENTS_INCLUDED



#include <map>

#include <raknet/BitStream.h>

#include "TimeStructs.h"
#include "VersionInfo.h"
#include "ClientInterface.h"



namespace network
{

namespace ranking
{

//------------------------------------------------------------------------------
/**
 *  Observable events for contacting ranking server and transmitting
 *  stats.
 */
enum MATCH_EVENTS_OBSERVABLE_EVENT
{
    MEOE_TRANSMISSION_FINISHED,
    MEOE_TRANSMISSION_FAILED
};


//------------------------------------------------------------------------------
enum MATCH_EVENT
{
    ME_MATCH_START         = 0,
    ME_MATCH_END           = 1,
    ME_PLAYER_CONNECTED    = 2,
    ME_PLAYER_DISCONNECTED = 3,
    ME_TEAM_CHANGE         = 4,
    ME_MATCH_SUMMARY       = 5,
    ME_KILL                = 9
};


//------------------------------------------------------------------------------
class MatchEventsConsumer
{
 public:
    virtual ~MatchEventsConsumer() {}
    
    virtual void onMatchStart(unsigned timestamp) = 0;
    virtual void onMatchEnd(unsigned timestamp) = 0;

    virtual void onPlayerConnected   (unsigned timestamp, uint32_t id, const SystemAddress & address) = 0;
    virtual void onPlayerDisconnected(unsigned timestamp, uint32_t id) = 0;

    virtual void onKill              (unsigned timestamp, uint32_t killer, uint32_t killed) = 0;
    
    virtual void onTeamChange        (unsigned timestamp, uint32_t id, uint8_t team_id) = 0;
};


//------------------------------------------------------------------------------
/**
 *  
 */
class MatchEvents : public ClientInterface
{
 public:
    MatchEvents();
    virtual ~MatchEvents();

    void setHosterAuthData(uint32_t id, uint32_t session_key);
    void getHosterAuthData(uint32_t & id, uint32_t & session_key) const;

    uint32_t getMatchId() const;

    void getMatchStartEnd(uint32_t & start, uint32_t & end) const;
    
    void setMapName(const std::string & name);
    const std::string & getMapName() const;

    void setServerIp(const SystemAddress & ip);
    const SystemAddress & getServerIp() const;
    
    void logPlayerConnected   (uint32_t player_id, uint32_t key, const SystemAddress & address);
    void logPlayerDisconnected(uint32_t player_id);

    void logKill(uint32_t killer, uint32_t killed);
    
    void logMatchStart();
    void logMatchEnd();

    void logTeamChange(uint32_t player_id, uint8_t team_id);
    
    void transmitToServer();

    void parseEvents(MatchEventsConsumer * consumer, int timestamp_offset) const;

    std::map<uint32_t,uint32_t> & getSessionKeys();
    
    static std::auto_ptr<MatchEvents> createFromBitstream(RakNet::BitStream & stream);
    

    virtual void writeStateToBitstream (RakNet::BitStream & stream) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream);

 protected:

    virtual bool handlePacket(Packet * packet);
    
    void shutdown();    

    unsigned logEvent(unsigned event);

    void onConnectFailed(const std::string & reason);

    void onMatchIdReceived(Observable* observable, unsigned);
    void onMatchIdRequestFailed(Observable* observable, void* ud, unsigned);
    
    void sendMatchEvents(const SystemAddress & dest);

    bool readAndValidateUserId(uint32_t & id, uint8_t event) const;
    
    virtual void parse(uint8_t event, unsigned timestamp, MatchEventsConsumer * consumer) const;

    network::ACCEPT_VERSION_CALLBACK_RESULT acceptVersionCallback(const VersionInfo & version);

    uint32_t hoster_id_;
    uint32_t hoster_session_key_;

    uint32_t match_id_; ///< The match id is requested from the
                        ///ranking server as soon as the match start
                        ///is logged.

    uint32_t match_start_;
    uint32_t match_end_;
    
    SystemAddress server_ip_;
    std::string map_name_;

    mutable RakNet::BitStream event_log_;
    
    std::map<uint32_t, uint32_t> session_key_; ///< the last received session keys for all players ever connected.    
};




 
}

}

#endif
