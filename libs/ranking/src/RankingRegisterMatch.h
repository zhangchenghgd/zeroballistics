
#ifndef RANKING_REGISTER_MATCH_INCLUDED
#define RANKING_REGISTER_MATCH_INCLUDED


#include <string>

#include "ClientInterface.h"


namespace network
{

namespace ranking
{


//------------------------------------------------------------------------------
enum REGISTER_MATCH_OBSERVABLE_EVENT
{
    RMOE_MATCH_ID_RECEIVED,
    RMOE_REGISTRATION_FAILED
};
    
//------------------------------------------------------------------------------
/**
 *  Contacts the ranking server to register a starting
 *  match. Authenticates with the given id and session key.
 *
 *  The registereds match id can be retrieved in the observable event
 *  handler, will perform suicide afterwards.
 */
class RegisterMatch : public ClientInterface
{
 public:

    RegisterMatch(uint32_t hoster_id, uint32_t session_key);
    
    virtual ~RegisterMatch();

    void connect();    

    unsigned getMatchId() const;
    
 protected:

    virtual bool handlePacket(Packet * packet);

    void sendRegistrationRequest(const SystemAddress & dest);
    void onConnectFailed(const std::string & reason);
    void onMatchIdReceived(RakNet::BitStream & stream);
    void onAuthorizationFailed();    
    
    void scheduleSuicide();
    void deleteSelf(void*);

    ACCEPT_VERSION_CALLBACK_RESULT acceptVersionCallback(const VersionInfo & version);

    uint32_t hoster_id_;
    uint32_t session_key_;
    uint32_t match_id_;


    bool suicide_scheduled_;    
};


} // namespace ranking

} // namespace network

#endif
