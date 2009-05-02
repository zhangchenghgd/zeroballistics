
#include "RankingRegisterMatch.h"

#include <raknet/RakNetworkFactory.h>
#include <raknet/BitStream.h>


#include "ParameterManager.h"
#include "Scheduler.h"

#include "RakAutoPacket.h"
#include "MessageIds.h"
#include "Ranking.h"
#include "NetworkUtils.h"

namespace network
{

namespace ranking
{


//------------------------------------------------------------------------------
RegisterMatch::RegisterMatch(uint32_t hoster_id, uint32_t session_key) :
    hoster_id_(hoster_id),
    session_key_(session_key),
    match_id_(INVALID_MATCH_ID),
    suicide_scheduled_(false)
{
}
    
//------------------------------------------------------------------------------
RegisterMatch::~RegisterMatch()
{
}


//------------------------------------------------------------------------------
void RegisterMatch::connect()
{
    ClientInterface::connect(s_params.get<std::vector<std::string> >("ranking_server.hosts"),
                             s_params.get<std::vector<unsigned> >   ("ranking_server.ports"),
                             2,
                             AcceptVersionCallbackClient(this, &RegisterMatch::acceptVersionCallback),
                             DEFAULT_SLEEP_TIMER,
                             DEFAULT_NETWORK_DT,
                             "ranking.pub");    
}


//------------------------------------------------------------------------------
unsigned RegisterMatch::getMatchId() const
{
    return match_id_;
}


//------------------------------------------------------------------------------
bool RegisterMatch::handlePacket(Packet * packet)
{
    RakNet::BitStream stream(&packet->data[1], packet->length-1, false);
    switch (packet->data[0])
    {
    case ID_DISCONNECTION_NOTIFICATION:
        if (!suicide_scheduled_)
        {
            // something is wrong if we are disconnected without
            // receiving either match id or authorization failed
            // messages...
            onConnectFailed("Connection to authentication server closed unexpectedly.");
        }
        break;
    case ID_CONNECTION_REQUEST_ACCEPTED:
        sendRegistrationRequest(packet->systemAddress);
        break;

    case ID_CONNECTION_ATTEMPT_FAILED:
        onConnectFailed("Failed to contact authentication server.");
        break;
    case ID_ALREADY_CONNECTED:
        onConnectFailed("Already connected!?");
        break;
    case ID_NO_FREE_INCOMING_CONNECTIONS:
        onConnectFailed("Server is busy. Try again in a moment.");
        break;
    case ID_CONNECTION_BANNED:
        onConnectFailed("You have been banned from the authentication server!");
        break;
    case VHPI_VERSION_MISMATCH:
        onConnectFailed("Client is not up to date.");
        break;
    case VHPI_TYPE_MISMATCH:
        onConnectFailed("Target server is not a valid authentication server.");
        break;
            
    case ID_RSA_PUBLIC_KEY_MISMATCH:
        onConnectFailed("Our public key stored for the ranking server is out of date.");
        break;


    case RPI_MATCH_ID:
        onMatchIdReceived(stream);
        break;

    case RPI_AUTHORIZATION_FAILED:
        onAuthorizationFailed();
        break;
            
    default:
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
void RegisterMatch::sendRegistrationRequest(const SystemAddress & dest)
{
    RakNet::BitStream stream;
    stream.Write((uint8_t)RPI_REQUEST_MATCH_ID);
    stream.Write(hoster_id_);
    stream.Write(session_key_);

    interface_->Send(&stream,
                     MEDIUM_PRIORITY,
                     RELIABLE_ORDERED,
                     0, dest, false);
}


//------------------------------------------------------------------------------
void RegisterMatch::onConnectFailed(const std::string & reason)
{
    emit(RMOE_REGISTRATION_FAILED, (void*)&reason);
    scheduleSuicide();
}

//------------------------------------------------------------------------------
void RegisterMatch::onMatchIdReceived(RakNet::BitStream & stream)
{
    stream.Read(match_id_);
    
    emit(RMOE_MATCH_ID_RECEIVED);

    scheduleSuicide();
}


//------------------------------------------------------------------------------
void RegisterMatch::onAuthorizationFailed()
{
    std::string msg = "Authorization failed.";
    emit(RMOE_REGISTRATION_FAILED, &msg);

    scheduleSuicide();    
}
    
//------------------------------------------------------------------------------
void RegisterMatch::scheduleSuicide()
{
    suicide_scheduled_ = true;
    s_scheduler.addEvent(SingleEventCallback(this, &RegisterMatch::deleteSelf),
                         0.0f,
                         NULL,
                         "RegisterMatch::deleteSelf",
                         &fp_group_);    
}


//------------------------------------------------------------------------------
void RegisterMatch::deleteSelf(void*)
{
    delete this;
}


//------------------------------------------------------------------------------
ACCEPT_VERSION_CALLBACK_RESULT RegisterMatch::acceptVersionCallback(const VersionInfo & version)
{
    if (version.type_ != VERSION_RANKING_SERVER.type_) return AVCR_TYPE_MISMATCH;

    // accept any ranking server version.
    return AVCR_ACCEPT;
}


}

    
}
