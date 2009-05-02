

#ifndef MASTER_MESSAGE_IDS_INCLDUED
#define MASTER_MESSAGE_IDS_INCLDUED


namespace network
{

namespace master
{



//------------------------------------------------------------------------------
enum MASTER_PACKET_ID
{
    MPI_REQUEST_AUTH_TOKEN = ID_USER_PACKET_ENUM,
    MPI_AUTH_TOKEN,
    
    // periodically must be sent by the game server to stay in the list. contains complete server info.
    MPI_HEARTBEAT,
    MPI_REMOVE_SERVER,

    
    MPI_REQUEST_SERVER_LIST, // sent by client to request the list of servers
    MPI_REQUEST_NAT_PUNCHTHROUGH, // client sends this to master
                                  // server, together with server
                                  // address he wants to connect to.
    

    MPI_GREATER_VERSION_AVAILABLE,
    MPI_SERVER_LIST,         // master answer
    
    MPI_LAST
};


} // namespace master

}

#endif
