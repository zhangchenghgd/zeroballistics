
#ifndef NETWORK_MULTIPLE_CONNECT_PLUGIN_INCLUDED
#define NETWORK_MULTIPLE_CONNECT_PLUGIN_INCLUDED


#include <vector>
#include <string>

#include <raknet/PluginInterface2.h>

namespace network
{
    

//------------------------------------------------------------------------------
/**
 *  Tries to connect to a more than one server. Retries for any given
 *  server if there were no free incoming connections. Performs
 *  suicide after successful connection / after all options have
 *  failed.
 */
class MultipleConnectPlugin : public PluginInterface2
{
 public:

    MultipleConnectPlugin();

    void connect(const std::vector<std::string> & hosts,
                 const std::vector<unsigned> & ports,
                 unsigned num_retries = 1);

    virtual void OnRakPeerShutdown();
    
    virtual PluginReceiveResult OnReceive(Packet *packet);
    
 protected:

    void connect(unsigned server_index);
    
    std::vector<std::string> hosts_;
    std::vector<unsigned>    ports_;
    std::vector<unsigned>    retries_;

    bool had_no_free_incoming_connections_;
    
    unsigned cur_server_;
};


} 
 
#endif
