
#ifndef NETWORK_VERSION_HANDSHAKE_INCLUDED
#define NETWORK_VERSION_HANDSHAKE_INCLUDED


#include <map>

#include <raknet/PluginInterface.h>

#include <loki/Functor.h>


#include "RegisteredFpGroup.h"
#include "VersionInfo.h"
#include "Scheduler.h"



namespace network
{

//------------------------------------------------------------------------------
enum ACCEPT_VERSION_CALLBACK_RESULT
{
    AVCR_ACCEPT,
    AVCR_TYPE_MISMATCH,
    AVCR_VERSION_MISMATCH
};

 
/// return whether to accept version, second parameter: which version
/// to report to client.
typedef Loki::Functor<ACCEPT_VERSION_CALLBACK_RESULT,
    LOKI_TYPELIST_2(const VersionInfo &, VersionInfo&) > AcceptVersionCallbackServer;
 
/// return whether to accept version
typedef Loki::Functor<ACCEPT_VERSION_CALLBACK_RESULT,
    LOKI_TYPELIST_1(const VersionInfo &) > AcceptVersionCallbackClient;
    


//------------------------------------------------------------------------------
/**
 *  Server side structure. Remember version info sent by client until
 *  client ack is received, to be apssed on.
 *
 *  Remember task_auto_close_ so auto close can be terminated when
 *  client disconnects / when connection is successfully established.
 */
class HandshakeInfo
{
 public:
    HandshakeInfo() : task_auto_close_(INVALID_TASK_HANDLE) {}
    HandshakeInfo(hTask close_task) : task_auto_close_(close_task) {}
    
    VersionInfo info_;
    hTask task_auto_close_;
};


    
//------------------------------------------------------------------------------
/**
 *  Version handshake can handle incoming (ID_NEW_INCOMING_CONNECTION)
 *  or outgoing (ID_CONNECTION_REQUEST_ACCEPTED) connections. If it
 *  handles outgoing, it commits suicide after connection is
 *  established ("client side").
 *
 * -) Client & Server automatically close connection after a timeout
 *    in case peer doesn't do version handshake.
 *  
 * -) After connection is established on client, client sends version
 *    to server.
 *
 * -) Server responds with a version determined by the accept_version_
 *    callback, or sends VHPI_VERSION_MISMATCH and closes the connection.
 *
 * -) Client responds with VHPI_VERSION_ACK, or with
 *    VHPI_VERSION_MISMATCH and closes the connection.
 *
 *
 *  VHPI_VERSION_INFO is passed through after successful
 *  handshake. VHPI_VERSION_MISMATCH contains the mismatching version.
 *
 *  Until version handshake is completed, this plugin eats all user
 *  data packets and other plugin's packets.
 *
 *  All in all, version handshake should happen transparently, with
 *  added VHPI_VERSION_INFO or VHPI_VERSION_MISMATCH messages.
 */
class VersionHandshakePlugin : public PluginInterface
{
 public:
    VersionHandshakePlugin(AcceptVersionCallbackServer cb);
    VersionHandshakePlugin(AcceptVersionCallbackClient cb);

    virtual void OnAttach(RakPeerInterface *peer);
    virtual void OnDetach(RakPeerInterface *peer);
    virtual void OnShutdown(RakPeerInterface *peer);
    
    virtual PluginReceiveResult OnReceive(RakPeerInterface *peer, Packet *packet);
    
 protected:

    void suicideIfNeccessary();
    
    void sendVersionInfo    (const SystemAddress & dest, const VersionInfo & info);
    void sendVersionAck     (const SystemAddress & dest);
    void sendMismatch       (const SystemAddress & dest, const VersionInfo & info, bool type_mismatch);

    void pushPacket(RakNet::BitStream & stream, const SystemAddress & address);

    hTask autoClose(const SystemAddress & address);
    void closeConnection(void * address);
    
    /// For incoming connections ("server side"). After connection is
    /// established, put it here so user data packets can be
    /// ignored. When version is received, put it here as well. After
    /// ack, report stored version via VHPI_VERSION_INFO, and remove
    /// entry.
    std::map<SystemAddress, HandshakeInfo> handshake_info_;
    
    bool server_side_;
    AcceptVersionCallbackServer accept_version_server_;
    AcceptVersionCallbackClient accept_version_client_;

    RakPeerInterface * interface_;

    RegisteredFpGroup fp_group_;
};


} 
 
#endif
