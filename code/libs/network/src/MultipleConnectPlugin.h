
#ifndef NETWORK_MULTIPLE_CONNECT_PLUGIN_INCLUDED
#define NETWORK_MULTIPLE_CONNECT_PLUGIN_INCLUDED


#include <vector>
#include <string>

#include <raknet/PluginInterface.h>

namespace network
{
    

//------------------------------------------------------------------------------
/**
 *  Tries to connect to a more than one server. Retries for any given
 *  server if there were no free incoming connections. Performs
 *  suicide after successful connection / after all options have
 *  failed.
 */
class MultipleConnectPlugin : public PluginInterface
{
 public:

    MultipleConnectPlugin();

    void connect(const std::vector<std::string> & hosts,
                 const std::vector<unsigned> & ports,
                 unsigned num_retries = 1);



    virtual void OnAttach(RakPeerInterface *peer);
    virtual void OnShutdown(RakPeerInterface *peer);
    
    virtual PluginReceiveResult OnReceive(RakPeerInterface *peer, Packet *packet);
    
 protected:

    void connect(unsigned server_index);
    
    std::vector<std::string> hosts_;
    std::vector<unsigned>    ports_;
    std::vector<unsigned>    retries_;

    bool had_no_free_incoming_connections_;
    
    unsigned cur_server_;
    
    RakPeerInterface * interface_;
};


} 
 
#endif
