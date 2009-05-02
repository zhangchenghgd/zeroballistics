
#ifndef RANKING_CLIENT_LOGON_INCLUDED
#define RANKING_CLIENT_LOGON_INCLUDED


#include <string>

#include "ClientInterface.h"


namespace network
{

namespace ranking
{


//------------------------------------------------------------------------------
enum CLIENT_LOGON_OBSERVABLE_EVENT
{
    CLOE_AUTHORIZATION_SUCCESSFUL, ///< use getSessionKey after successful logon.
    CLOE_AUTHORIZATION_FAILED,
    CLOE_CONNECT_FAILED
};
    
//------------------------------------------------------------------------------
/**
 *  Handles logging on the ranking server for the client. Successful
 *  logon results in a session key, which will be used to contact any
 *  game server.
 *
 *  Will commit suicide after observable event is emitted.
 */
class ClientLogon : public ClientInterface
{
 public:

    ClientLogon(const std::string & name,
                const std::string & passwd_hash);
    
    virtual ~ClientLogon();

    void connect();
    
    uint32_t getUserId() const;
    uint32_t getSessionKey() const;
    

 protected:

    virtual bool handlePacket(Packet * packet);

    void sendCredentials(const SystemAddress & dest);
    void onConnectFailed(const std::string & reason);
    void onSessionKeyReceived (RakNet::BitStream & stream);
    void onAuthorizationFailed(RakNet::BitStream & stream);

    void scheduleSuicide();
    void deleteSelf(void*);

    ACCEPT_VERSION_CALLBACK_RESULT acceptVersionCallback(const VersionInfo & version);
    
    std::string name_;
    std::string pwd_hash_;
    
    uint32_t user_id_;
    uint32_t session_key_;
    
    bool suicide_scheduled_;    
};


} // namespace ranking

} // namespace network

#endif
