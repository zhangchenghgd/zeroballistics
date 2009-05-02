
#include "RankingMatchEvents.h"


#include <raknet/RakNetworkFactory.h>
#include <raknet/MessageIdentifiers.h>

#include "Log.h"
#include "NetworkUtils.h"
#include "RakAutoPacket.h"

#include "RankingMatchEventsSoccer.h"
#include "RankingRegisterMatch.h"
#include "RankingStatisticsSoccer.h" /// XXXXX factory instead

#include "ParameterManager.h"
#include "Scheduler.h"

#include "Ranking.h"
#include "MessageIds.h"
#include "MultipleConnectPlugin.h"

namespace network
{
    
namespace ranking
{

const unsigned NUM_CONNECT_RETRIES = 4;

    
//------------------------------------------------------------------------------
MatchEvents::MatchEvents() :
    hoster_id_(INVALID_USER_ID),
    hoster_session_key_(INVALID_SESSION_KEY),
    match_id_(INVALID_MATCH_ID),
    match_start_(0),
    match_end_(0),
    server_ip_(UNASSIGNED_SYSTEM_ADDRESS)
{
}



//------------------------------------------------------------------------------
MatchEvents::~MatchEvents()
{
}


//------------------------------------------------------------------------------
/**
 *  Authentication data is needed to request match id from the ranking
 *  server.
 */
void MatchEvents::setHosterAuthData(uint32_t id, uint32_t session_key)
{
    hoster_id_          = id;
    hoster_session_key_ = session_key;
}


//------------------------------------------------------------------------------
void MatchEvents::getHosterAuthData(uint32_t & id, uint32_t & session_key) const
{
    id = hoster_id_;
    session_key = hoster_session_key_;
}




//------------------------------------------------------------------------------
uint32_t MatchEvents::getMatchId() const
{
    return match_id_;
}


//------------------------------------------------------------------------------
/**
 *  Match start and end timestamps (game server's time).
 */
void MatchEvents::getMatchStartEnd(uint32_t & start, uint32_t & end) const
{
    start = match_start_;
    end   = match_end_;
}



//------------------------------------------------------------------------------
void MatchEvents::setMapName(const std::string & name)
{
    map_name_ = name;
}

//------------------------------------------------------------------------------
const std::string & MatchEvents::getMapName() const
{
    return map_name_;
}


//------------------------------------------------------------------------------
void MatchEvents::setServerIp(const SystemAddress & ip)
{
    server_ip_ = ip;
}


//------------------------------------------------------------------------------
const SystemAddress & MatchEvents::getServerIp() const
{
    return server_ip_;
}




//------------------------------------------------------------------------------
/**
 *  Sets the session key for a specific player. This is independent
 *  from connect / disconnect, as we need the information until stats
 *  are transmitted. If a client connects twice, his new session key
 *  is used.
 */
void MatchEvents::logPlayerConnected(uint32_t player_id,
                                   uint32_t key,
                                   const SystemAddress & address)
{
    session_key_[player_id] = key;

    logEvent(ME_PLAYER_CONNECTED);
    event_log_.Write(player_id);
    event_log_.Write(address);
}


//------------------------------------------------------------------------------
void MatchEvents::logPlayerDisconnected(uint32_t player_id)
{
    logEvent(ME_PLAYER_DISCONNECTED);
    event_log_.Write(player_id);
}


//------------------------------------------------------------------------------
void MatchEvents::logKill(uint32_t killer, uint32_t killed)
{    
    logEvent(ME_KILL);
    event_log_.Write(killer);
    event_log_.Write(killed);
}


//------------------------------------------------------------------------------
void MatchEvents::logMatchStart()
{
    match_start_ = logEvent(ME_MATCH_START);

    // Request match id. This opens a new match record on the ranking
    // server, hoster is punished if match is not completed.
    RegisterMatch * reg = new RegisterMatch(hoster_id_, hoster_session_key_);
    reg->addObserver(ObserverCallbackFun2(this, &MatchEvents::onMatchIdReceived),
                     RMOE_MATCH_ID_RECEIVED, &fp_group_);
    reg->addObserver(ObserverCallbackFunUserData(this, &MatchEvents::onMatchIdRequestFailed),
                     RMOE_REGISTRATION_FAILED, &fp_group_);
    reg->connect();    
}


//------------------------------------------------------------------------------
void MatchEvents::logMatchEnd()
{
    match_end_ = logEvent(ME_MATCH_END);
}


//------------------------------------------------------------------------------
void MatchEvents::logTeamChange(uint32_t player_id, uint8_t team_id)
{
    logEvent(ME_TEAM_CHANGE);
    event_log_.Write(player_id);
    event_log_.Write(team_id);
}


//------------------------------------------------------------------------------
/**
 *  First, establish a secure connection to ranking server, do version
 *  handshake.
 */
void MatchEvents::transmitToServer()
{
    connect(s_params.get<std::vector<std::string> >("ranking_server.hosts"),
            s_params.get<std::vector<unsigned> >   ("ranking_server.ports"),
            NUM_CONNECT_RETRIES,
            AcceptVersionCallbackClient(this, &MatchEvents::acceptVersionCallback),
            DEFAULT_SLEEP_TIMER,
            DEFAULT_NETWORK_DT,
            "ranking.pub");
}



//------------------------------------------------------------------------------
/**
 *  Events will be ignored for players not in session_key_.
 */
void MatchEvents::parseEvents(MatchEventsConsumer * consumer, int timestamp_offset) const
{
    uint8_t event;
    uint32_t timestamp;
    
    while(event_log_.Read(event))
    {
        event_log_.Read(timestamp);
        
        parse(event, (int)timestamp+(int)timestamp_offset, consumer);
    }

    event_log_.ResetReadPointer();
}


//------------------------------------------------------------------------------
std::map<uint32_t,uint32_t> & MatchEvents::getSessionKeys()
{
    return session_key_;
}



//------------------------------------------------------------------------------
std::auto_ptr<MatchEvents> MatchEvents::createFromBitstream(RakNet::BitStream & stream)
{
    std::string game;
    readFromBitstream(stream, game);
    stream.ResetReadPointer();

    std::auto_ptr<MatchEvents> ret;    
    if (game == MatchEventsSoccer::getGameName())
    {
        ret.reset(new MatchEventsSoccer());
        ret->readStateFromBitstream(stream);
    } else
    {
        Exception e("Unknown game in MatchEvents::createFromBitstream: ");
        e << game;
        throw e;
    }

    return ret;
}
    


//------------------------------------------------------------------------------
void MatchEvents::writeStateToBitstream (RakNet::BitStream & stream) const
{
    stream.Write(hoster_id_);
    stream.Write(hoster_session_key_);
    stream.Write(match_id_);

    stream.Write(match_start_);
    stream.Write(match_end_);
    
    stream.Write(server_ip_);
    writeToBitstream(stream, map_name_);
    
    stream.Write((uint8_t)session_key_.size());
    for (std::map<uint32_t, uint32_t>::const_iterator it = session_key_.begin();
         it != session_key_.end();
         ++it)
    {
        stream.Write(it->first);
        stream.Write(it->second);
    }

    writeToBitstream(stream, event_log_);
}

//------------------------------------------------------------------------------
void MatchEvents::readStateFromBitstream(RakNet::BitStream & stream)
{
    stream.Read(hoster_id_);
    stream.Read(hoster_session_key_);
    stream.Read(match_id_);

    stream.Read(match_start_);
    stream.Read(match_end_);

    stream.Read(server_ip_);
    readFromBitstream(stream, map_name_);

    uint8_t num_keys;
    stream.Read(num_keys);
    for (unsigned i=0; i<num_keys; ++i)
    {
        uint32_t id, key;
        stream.Read(id);
        stream.Read(key);
        session_key_[id] = key;
    }

    readFromBitstream(stream, event_log_);
}


//------------------------------------------------------------------------------
bool MatchEvents::handlePacket(Packet * packet)
{
    RakNet::BitStream stream(&packet->data[1], packet->length-1, false);
    switch (packet->data[0])
    {
    case ID_NO_FREE_INCOMING_CONNECTIONS:            
        onConnectFailed("No free incoming connections.");
        break;
    case ID_DISCONNECTION_NOTIFICATION:
        onConnectFailed("Connection to ranking server closed unexpectedly.");
        break;
    case ID_CONNECTION_LOST:
        onConnectFailed("Connection to ranking server closed unexpectedly.");
        break;
    case ID_CONNECTION_ATTEMPT_FAILED:
        onConnectFailed("Failed to connect to ranking server");
        break;
    case ID_ALREADY_CONNECTED:
        onConnectFailed("Already connected!?!");
        break;
    case ID_CONNECTION_BANNED:
        onConnectFailed("We have been banned from the ranking server.");
        break;
            
    case ID_CONNECTION_REQUEST_ACCEPTED:
        sendMatchEvents(packet->systemAddress);
        break;

    case ID_RSA_PUBLIC_KEY_MISMATCH:
        onConnectFailed("Our public key stored for the ranking server is invalid.");
        break;
            
    case RPI_STATS_REJECTED:
    {
        std::string reason;
        readFromBitstream(stream, reason);
        shutdown();
        emit(MEOE_TRANSMISSION_FAILED, &reason);
        break;
    }        
    case RPI_STATS_ACK:
    {
        s_log << "Stats transmission finished.\n";

        uint8_t num_rejected_players;
        stream.Read(num_rejected_players);

        if (num_rejected_players)
        {
            s_log << Log::warning << "Some player stats were rejected: ";
            for (unsigned i=0; i<num_rejected_players; ++i)
            {
                uint32_t rej_id;
                stream.Read(rej_id);
                s_log << rej_id << " ";
            }
            s_log << "\n";
        }

        // XXXX Factory goes here...
        StatisticsSoccer stats;
        stats.readFromBitstream(stream);

        shutdown();
        emit(MEOE_TRANSMISSION_FINISHED, &stats);
        break;
    }
            
    default:
        return false;
        break;
    }
    return true;
}



//------------------------------------------------------------------------------
void MatchEvents::shutdown()
{
    if (interface_)
    {
        interface_->Shutdown(500);
    }    
}



//------------------------------------------------------------------------------
/**
 *  Logs an event with timestamp since match start.
 */
unsigned MatchEvents::logEvent(unsigned event)
{
    unsigned timestamp = (uint32_t)time(NULL);

    event_log_.Write((uint8_t)event);
    event_log_.Write(timestamp);
    
    return timestamp;
}


//------------------------------------------------------------------------------
void MatchEvents::onConnectFailed(const std::string & reason)
{
    shutdown();
    emit(MEOE_TRANSMISSION_FAILED, (void*)&reason);
}

//------------------------------------------------------------------------------
void MatchEvents::onMatchIdReceived(Observable* o, unsigned)
{
    RegisterMatch * reg = (RegisterMatch*)o;
    match_id_ = reg->getMatchId();
}

//------------------------------------------------------------------------------
void MatchEvents::onMatchIdRequestFailed(Observable* observable, void* ud, unsigned ev)
{
    const std::string & reason = *(std::string*)(ud);
    s_log << Log::warning
          << "Failed to register a match ID: "
          << reason
          << "\n";
}


//------------------------------------------------------------------------------
void MatchEvents::sendMatchEvents(const SystemAddress & dest)
{
    RakNet::BitStream stream;

    stream.Write((uint8_t)RPI_GAME_STATS);
    writeStateToBitstream(stream);

    interface_->Send(&stream,
                     MEDIUM_PRIORITY,
                     RELIABLE_ORDERED,
                     0, dest, false);    
}



//------------------------------------------------------------------------------
/**
 *  Reads a user id from the event_log_, and checks its existence in
 *  the session_key_ map.
 *
 *  \return Whether the id exists in session_key_.
 */
bool MatchEvents::readAndValidateUserId(uint32_t & id, uint8_t event) const
{
    event_log_.Read(id);

    if (id == 0) return false;
    
    bool ret = session_key_.find(id) != session_key_.end();

    if (!ret)
    {
        s_log << Log::debug('l')
              << "Ignoring event "
              << (unsigned)event
              <<" for player "
              << id
              << " as he's not in connection_info_\n";
    }
    
    return ret;
}


//-----------------------------------------------------------------------------
/**
 *  Player-specific events are not reported if player is not in the
 *  session_key_ map.
 */
void MatchEvents::parse(uint8_t event, unsigned timestamp, MatchEventsConsumer * consumer) const
{
    uint32_t user_id,user_id2;
    bool valid;
    SystemAddress address;
    
    switch (event)
    {
    case ME_MATCH_START:
        consumer->onMatchStart(timestamp);
        break;

    case ME_MATCH_END:
        consumer->onMatchEnd(timestamp);
        break;
        
    case ME_PLAYER_CONNECTED:
        valid = readAndValidateUserId(user_id, event);
        event_log_.Read(address);

        if (valid) consumer->onPlayerConnected(timestamp, user_id, address);
        break;

    case ME_PLAYER_DISCONNECTED:
        valid = readAndValidateUserId(user_id, event);

        if (valid) consumer->onPlayerDisconnected(timestamp, user_id);
        break;

    case ME_KILL:
        valid =  readAndValidateUserId(user_id, event);
        valid &= readAndValidateUserId(user_id2, event);

        if (valid) consumer->onKill(timestamp, user_id, user_id2);
        break;
        
    case ME_TEAM_CHANGE:
        valid = readAndValidateUserId(user_id, event);
        
        uint8_t team_id;
        event_log_.Read(team_id);

        if (valid) consumer->onTeamChange(timestamp, user_id, team_id);        
        break;
       
    default:
        Exception e;
        e << "Unknown game event in MatchEvents::parse: "
          << (unsigned)event;
        throw e;
    }
}


//------------------------------------------------------------------------------
ACCEPT_VERSION_CALLBACK_RESULT MatchEvents::acceptVersionCallback(const VersionInfo & version)
{
    if (version.type_ != VERSION_RANKING_SERVER.type_) return AVCR_TYPE_MISMATCH;

    // accept any ranking server version.
    return AVCR_ACCEPT;
}


}


}
