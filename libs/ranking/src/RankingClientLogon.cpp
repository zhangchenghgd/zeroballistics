
#include "RankingClientLogon.h"

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
/**
 *  \param passwd_hash: We don't want to save the user passwor din
 *  cleartext, so work with hash from beginning.
 */
ClientLogon::ClientLogon(const std::string & name,
                         const std::string & passwd_hash) :
    name_(name),
    pwd_hash_(passwd_hash),
    user_id_    (INVALID_USER_ID),
    session_key_(INVALID_SESSION_KEY),
    suicide_scheduled_(false)
{
}
    
//------------------------------------------------------------------------------
ClientLogon::~ClientLogon()
{
}


//------------------------------------------------------------------------------
void ClientLogon::connect()
{
    ClientInterface::connect(s_params.get<std::vector<std::string> >("ranking_server.hosts"),
                             s_params.get<std::vector<unsigned> >   ("ranking_server.ports"),
                             2,
                             AcceptVersionCallbackClient(this, &ClientLogon::acceptVersionCallback),
                             DEFAULT_SLEEP_TIMER,
                             DEFAULT_NETWORK_DT,
                             "ranking.pub");    
}


//------------------------------------------------------------------------------
uint32_t ClientLogon::getUserId() const
{
    return user_id_;
}


//------------------------------------------------------------------------------
uint32_t ClientLogon::getSessionKey() const
{
    return session_key_;
}


//------------------------------------------------------------------------------
bool ClientLogon::handlePacket(Packet * packet)
{
    RakNet::BitStream stream(&packet->data[1], packet->length-1, false);
    switch (packet->data[0])
    {
    case ID_DISCONNECTION_NOTIFICATION:
        if (!suicide_scheduled_)
        {
            // something is wrong if we are disconnected without
            // receiving either session key or authorization
            // failed messages...
            onConnectFailed("Connection to authentication server closed unexpectedly.");
        }
        break;
    case ID_CONNECTION_REQUEST_ACCEPTED:
        sendCredentials(packet->systemAddress);
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
            
    case RPI_SESSION_KEY:
        onSessionKeyReceived(stream);
        break;

    case RPI_AUTHORIZATION_FAILED:
        onAuthorizationFailed(stream);
        break;
            
    default:
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
void ClientLogon::sendCredentials(const SystemAddress & dest)
{
    RakNet::BitStream stream;
    stream.Write((uint8_t)RPI_CREDENTIALS);
    writeToBitstream(stream, name_);
    writeToBitstream(stream, pwd_hash_);

    interface_->Send(&stream,
                     MEDIUM_PRIORITY,
                     RELIABLE_ORDERED,
                     0, dest, false);
}


//------------------------------------------------------------------------------
void ClientLogon::onConnectFailed(const std::string & reason)
{
    emit(CLOE_CONNECT_FAILED, (void*)&reason);
    scheduleSuicide();
}

//------------------------------------------------------------------------------
/**
 *  Our job is done. Emit an observable event, then die gracefully.
 */
void ClientLogon::onSessionKeyReceived(RakNet::BitStream & stream)
{
    stream.Read(user_id_);
    stream.Read(session_key_);
    
    emit(CLOE_AUTHORIZATION_SUCCESSFUL);

    scheduleSuicide();
}


//------------------------------------------------------------------------------
/**
 *  Couldn't authorize - emit observable event and kill self.
 */
void ClientLogon::onAuthorizationFailed(RakNet::BitStream & stream)
{

    std::string msg;
    readFromBitstream(stream, msg);
    
    s_log << "ranking server says: "
          << msg
          << "\n";
    
    emit(CLOE_AUTHORIZATION_FAILED, &msg);

    scheduleSuicide();
}



//------------------------------------------------------------------------------
void ClientLogon::scheduleSuicide()
{
    suicide_scheduled_ = true;
    s_scheduler.addEvent(SingleEventCallback(this, &ClientLogon::deleteSelf),
                         0.0f,
                         NULL,
                         "ClientLogon::deleteSelf",
                         &fp_group_);
}


//------------------------------------------------------------------------------
void ClientLogon::deleteSelf(void*)
{
    delete this;
}


//------------------------------------------------------------------------------
ACCEPT_VERSION_CALLBACK_RESULT ClientLogon::acceptVersionCallback(const VersionInfo & version)
{
    if (version.type_ != VERSION_RANKING_SERVER.type_) return AVCR_TYPE_MISMATCH;

    // accept any ranking server version.
    return AVCR_ACCEPT;
}


}

    
}
