
#ifndef TOOLBOX_RAK_AUTOPACKET_INCLUDED
#define TOOLBOX_RAK_AUTOPACKET_INCLUDED


namespace network
{

const unsigned UNRELIABLE_PACKET_TIMEOUT = 400;
    
//------------------------------------------------------------------------------
class RakAutoPacket
{
 public:
    RakAutoPacket(RakPeerInterface * rak_peer_interface) :
        interface_(rak_peer_interface),
        packet_(NULL) { }

    virtual ~RakAutoPacket()
        {
            if (packet_) interface_->DeallocatePacket(packet_);
        }

    bool receive()
        {
            if (packet_) interface_->DeallocatePacket(packet_);

            do
            {
                packet_ = interface_->Receive();
            } while (packet_ && packet_->length == 0);
            
            return packet_ != NULL;
        }

    
    Packet * operator->()
        {
            return packet_;
        }

    operator Packet*()
        {
            return packet_;
        }
    
 protected:
    RakPeerInterface * interface_;
    Packet * packet_;
};


} 

#endif
