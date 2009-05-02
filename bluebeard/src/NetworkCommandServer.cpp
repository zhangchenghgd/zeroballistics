//******************** Commands issued by server ********************//

#include "NetworkCommandServer.h"

#include <raknet/RakPeerInterface.h>
#include <raknet/RakNetworkFactory.h>
#include <raknet/GetTime.h>

#include "GameState.h"
#include "Log.h"
#include "GameLogicServer.h"
#include "Controllable.h"

#include "ClassLoader.h"

#ifndef DEDICATED_SERVER
#include "GameLogicClient.h"
#include "PuppetMasterClient.h"
#include "Profiler.h"
#endif


//------------------------------------------------------------------------------
namespace network
{



//********** VersionInfoCmd **********//


//------------------------------------------------------------------------------
VersionInfoCmd::VersionInfoCmd()
{
}


//------------------------------------------------------------------------------
VersionInfoCmd::VersionInfoCmd(const VersionInfo & v) :
    version_(v)
{
}

//------------------------------------------------------------------------------
void VersionInfoCmd::execute(PuppetMasterClient * master)
{
    assert(false);
}

//------------------------------------------------------------------------------
VersionInfo VersionInfoCmd::getVersion() const
{
    return version_;
}


//------------------------------------------------------------------------------
void VersionInfoCmd::getNetworkOptions(PacketReliability & reliability,
                                       PacketPriority    & priority,
                                       unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = HIGH_PRIORITY;
    channel     = NC_GAMESTATE;
}


//------------------------------------------------------------------------------
void VersionInfoCmd::writeToBitstream(RakNet::BitStream & stream)
{
    stream.Write((char)TPI_VERSION_INFO_SERVER);
    stream.Write(version_.game_);    
    stream.Write(version_.major_);    
    stream.Write(version_.minor_);    
}


//------------------------------------------------------------------------------
void VersionInfoCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    stream.Read(version_.game_);
    stream.Read(version_.major_);    
    stream.Read(version_.minor_);    
}

//********** LoadLevelCmd **********//

//------------------------------------------------------------------------------
LoadLevelCmd::LoadLevelCmd()
{
}


//------------------------------------------------------------------------------
LoadLevelCmd::LoadLevelCmd(const std::string & map_name,
                           const std::string & game_logic_type) :
    map_name_(map_name),
    game_logic_type_(game_logic_type)
{
}



//------------------------------------------------------------------------------
void LoadLevelCmd::execute(PuppetMasterClient * master)
{
#ifndef DEDICATED_SERVER
    master->loadLevel(map_name_, game_logic_type_);
    s_log << Log::debug('n')
          << "LoadLevelCmd executed: map "
          << map_name_
          << ", game logic "
          << game_logic_type_
          << "\n";
#endif
}



//------------------------------------------------------------------------------
void LoadLevelCmd::getNetworkOptions(PacketReliability & reliability,
                                     PacketPriority    & priority,
                                     unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}

//------------------------------------------------------------------------------
void LoadLevelCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_LOAD_LEVEL);
    network::writeToBitstream(stream, map_name_);
    network::writeToBitstream(stream, game_logic_type_);
}

//------------------------------------------------------------------------------
void LoadLevelCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    network::readFromBitstream(stream, map_name_);
    network::readFromBitstream(stream, game_logic_type_);
}

    


//********** CreatePlayerCmd **********//

//------------------------------------------------------------------------------
CreatePlayerCmd::CreatePlayerCmd()
{
}


//------------------------------------------------------------------------------
CreatePlayerCmd::CreatePlayerCmd(const SystemAddress & id) :
    id_(id)
{
}



//------------------------------------------------------------------------------
void CreatePlayerCmd::execute(PuppetMasterClient * master)
{
#ifndef DEDICATED_SERVER
    master->addRemotePlayer(id_);
    s_log << Log::debug('n') << "CreatePlayerCmd executed: "
          << id_ << "\n";
#endif
}



//------------------------------------------------------------------------------
void CreatePlayerCmd::getNetworkOptions(PacketReliability & reliability,
                                        PacketPriority    & priority,
                                        unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}

//------------------------------------------------------------------------------
void CreatePlayerCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_CREATE_PLAYER);
    stream.Write(id_);
}

//------------------------------------------------------------------------------
void CreatePlayerCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    stream.Read(id_);
}


//********** DeletePlayerCmd **********//


//------------------------------------------------------------------------------
DeletePlayerCmd::DeletePlayerCmd()
{
}

//------------------------------------------------------------------------------
DeletePlayerCmd::DeletePlayerCmd(const SystemAddress & id) : id_(id)
{
}


//------------------------------------------------------------------------------
void DeletePlayerCmd::execute(PuppetMasterClient * master)
{    
#ifndef DEDICATED_SERVER
    master->deleteRemotePlayer(id_);
    s_log << Log::debug('n') << "DeletePlayerCmd executed: " << id_ << "\n";
#endif
}


//------------------------------------------------------------------------------
void DeletePlayerCmd::getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}


//------------------------------------------------------------------------------
void DeletePlayerCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_DELETE_PLAYER);
    stream.Write(id_);
}

//------------------------------------------------------------------------------
void DeletePlayerCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    stream.Read(id_);
}

//********** SetControllableCmd **********//



//------------------------------------------------------------------------------
SetControllableCmd::SetControllableCmd()
{
}

//------------------------------------------------------------------------------
SetControllableCmd::SetControllableCmd(const SystemAddress & id,
                                       Controllable * controllable) :
    player_id_(id),
    controllable_id_(controllable ? controllable->getId() : INVALID_GAMEOBJECT_ID)
{
}


//------------------------------------------------------------------------------
void SetControllableCmd::execute(PuppetMasterClient * master)
{    
#ifndef DEDICATED_SERVER
    master->setControllable(player_id_, controllable_id_);
    s_log << Log::debug('n') << "SetControllableCmd executed: "
          << player_id_ << " is assigned to "
          << controllable_id_ << "\n";
#endif
}



//------------------------------------------------------------------------------
void SetControllableCmd::getNetworkOptions(PacketReliability & reliability,
                                           PacketPriority    & priority,
                                           unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}


//------------------------------------------------------------------------------
void SetControllableCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_SET_CONTROLLABLE);
    stream.Write(player_id_);
    stream.Write(controllable_id_);
}

//------------------------------------------------------------------------------
void SetControllableCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    stream.Read(player_id_);
    stream.Read(controllable_id_);
}



//********** SetControllableStateCmd **********//


//------------------------------------------------------------------------------
SetControllableStateCmd::SetControllableStateCmd()
{
}

//------------------------------------------------------------------------------
SetControllableStateCmd::SetControllableStateCmd(uint8_t sequence_number,
                                                 const Controllable * object) :
    sequence_number_(sequence_number)
{
    object->writeStateToBitstream(state_, OST_CORE | OST_CLIENT_SIDE_PREDICTION);
}


//------------------------------------------------------------------------------
void SetControllableStateCmd::execute(PuppetMasterClient * master)
{
#ifndef DEDICATED_SERVER
    master->getLocalPlayer()->serverCorrection(sequence_number_, state_);
#endif
}

//------------------------------------------------------------------------------
void SetControllableStateCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_SET_CONTROLLABLE_STATE);
    stream.Write(sequence_number_);

    network::writeToBitstream(stream, state_);
}

//------------------------------------------------------------------------------
void SetControllableStateCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    
    stream.Read(packet_id);
    stream.Read(sequence_number_);

    network::readFromBitstream(stream, state_);
}


//------------------------------------------------------------------------------
void SetControllableStateCmd::getNetworkOptions(PacketReliability & reliability,
                                                PacketPriority    & priority,
                                                unsigned          & channel)
{
    reliability = UNRELIABLE;
    priority    = HIGH_PRIORITY;
    channel     = NC_DONTCARE;
}


//------------------------------------------------------------------------------
CreateGameObjectCmd::CreateGameObjectCmd()
{
}

//------------------------------------------------------------------------------
CreateGameObjectCmd::CreateGameObjectCmd(GameObject * object) :
    id_       (object->getId()),
    type_     (object->getType())
{
    object->writeInitValuesToBitstream(object_init_values_stream_);

    s_log << Log::debug('n')
          << "CreateGameObjectCmd constructed for "
          << *object
          << "\n";
}

//------------------------------------------------------------------------------
void CreateGameObjectCmd::execute(PuppetMasterClient * master)
{
#ifndef DEDICATED_SERVER
    PROFILE(CreateGameObjectCmd::execute);
    
    std::auto_ptr<GameObject> new_object(s_game_object_loader.create(type_));
    
    new_object->setId(id_);

    try
    {
        new_object->readInitValuesFromBitstream(object_init_values_stream_, master->getGameState(), timestamp_);
        master->addGameObject(new_object.release());
    } catch (Exception & e)
    {
        e.addHistory("CreateGameObjectCmd::execute()");
        throw e;
    }

#endif
}


//------------------------------------------------------------------------------
void CreateGameObjectCmd::getNetworkOptions(PacketReliability & reliability,
                                            PacketPriority    & priority,
                                            unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}

//------------------------------------------------------------------------------
void CreateGameObjectCmd::writeToBitstream (RakNet::BitStream & stream)
{
    timestamp_ = RakNet::GetTime();
    
    stream.Write((char)ID_TIMESTAMP);
    stream.Write(timestamp_);

    
    stream.Write((char)TPI_CREATE_GAME_OBJECT);
    stream.Write(id_);

    network::writeToBitstream(stream, type_);

    network::writeToBitstream(stream, object_init_values_stream_);
    network::writeToBitstream(stream, object_state_stream_);

}

//------------------------------------------------------------------------------
void CreateGameObjectCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;

    stream.Read(packet_id);
    stream.Read(timestamp_);

    stream.Read(packet_id);
    stream.Read(id_);

    network::readFromBitstream(stream, type_);

    network::readFromBitstream(stream, object_init_values_stream_);
    network::readFromBitstream(stream, object_state_stream_);
}


//------------------------------------------------------------------------------
DeleteGameObjectCmd::DeleteGameObjectCmd()
{
}

//------------------------------------------------------------------------------
DeleteGameObjectCmd::DeleteGameObjectCmd(uint16_t id) :
    id_(id)
{
}


//------------------------------------------------------------------------------
void DeleteGameObjectCmd::execute(PuppetMasterClient * master)
{
#ifndef DEDICATED_SERVER
    master->deleteGameObject(id_);

    s_log << Log::debug('n')
          << "DeleteGameObjectCmd executed for "
          << id_
          << "\n";
#endif
}

//------------------------------------------------------------------------------
void DeleteGameObjectCmd::getNetworkOptions(PacketReliability & reliability,
                                            PacketPriority    & priority,
                                            unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}

//------------------------------------------------------------------------------
void DeleteGameObjectCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_DELETE_GAME_OBJECT);
    stream.Write(id_);
}

//------------------------------------------------------------------------------
void DeleteGameObjectCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    stream.Read(id_);
}


//------------------------------------------------------------------------------
SetGameObjectStateCmd::SetGameObjectStateCmd()
{
}

//------------------------------------------------------------------------------
SetGameObjectStateCmd::SetGameObjectStateCmd(const GameObject * object,
                                             OBJECT_STATE_TYPE type) :
    type_(type),
    id_(object->getId())
{
    object->writeStateToBitstream(object_state_stream_, type_);
}

//------------------------------------------------------------------------------
void SetGameObjectStateCmd::execute(PuppetMasterClient * master)
{
#ifndef DEDICATED_SERVER
    GameObject * target = master->getGameState()->getGameObject(id_);
    
    if (!target)
    {
        s_log << Log::debug('n')
              << "Got SetGameObjectStateCmd for nonexisting object "
              << id_ << "\n";
        return;
    } else
    {
//         s_log << Log::debug('b')
//               << "Received SetGameObjectStateCmd for "
//               << *target
//               << "\n";
    }

    target->readStateFromBitstream(object_state_stream_, type_, timestamp_);
   
    // set local players latency value, used for calculations on client
    uint32_t cur_time = RakNet::GetTime();
    if (cur_time <= timestamp_) return;
    float latency = (float)(cur_time - timestamp_) * 0.001f;

    master->getLocalPlayer()->setNetworkDelay(latency);
#endif
}


//------------------------------------------------------------------------------
void SetGameObjectStateCmd::getNetworkOptions(PacketReliability & reliability,
                                              PacketPriority    & priority,
                                              unsigned          & channel)
{
    if (type_ & OST_EXTRA)
    {
        reliability = RELIABLE_ORDERED;
        priority    = MEDIUM_PRIORITY;
        channel     = NC_GAMESTATE;
    } else
    {
        reliability = UNRELIABLE;
        priority    = LOW_PRIORITY;
        channel     = NC_DONTCARE;
    }
}

//------------------------------------------------------------------------------
void SetGameObjectStateCmd::writeToBitstream (RakNet::BitStream & stream)
{
    if (object_state_stream_.GetNumberOfBitsUsed() == 0) return;

    timestamp_ = RakNet::GetTime();
    
    stream.Write((char)ID_TIMESTAMP);
    stream.Write(timestamp_);

    if (type_ == OST_CORE)  stream.Write((uint8_t)TPI_SET_GAME_OBJECT_STATE_CORE); else
    if (type_ == OST_EXTRA) stream.Write((uint8_t)TPI_SET_GAME_OBJECT_STATE_EXTRA); else
    if (type_ == OST_BOTH)  stream.Write((uint8_t)TPI_SET_GAME_OBJECT_STATE_BOTH);

    stream.Write(id_);

    network::writeToBitstream(stream, object_state_stream_);
}

//------------------------------------------------------------------------------
void SetGameObjectStateCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;

    stream.Read(packet_id);
    stream.Read(timestamp_);

    stream.Read(packet_id);
    stream.Read(id_);

    network::readFromBitstream(stream, object_state_stream_);

    if (packet_id == TPI_SET_GAME_OBJECT_STATE_CORE)  type_ = OST_CORE;  else
    if (packet_id == TPI_SET_GAME_OBJECT_STATE_EXTRA) type_ = OST_EXTRA; else
    if (packet_id == TPI_SET_GAME_OBJECT_STATE_BOTH)  type_ = OST_BOTH;  else assert(false);
}


//********** StringMessageCmd **********//

//------------------------------------------------------------------------------
StringMessageCmd::StringMessageCmd()
{
}


//------------------------------------------------------------------------------
StringMessageCmd::StringMessageCmd(STRING_MESSAGE_TYPE type,
                                   const std::string & msg,
                                   const SystemAddress & pid1) :
    type_(type),
    msg_(msg),
    pid1_(pid1)
{
}


//------------------------------------------------------------------------------
/**
 *  
 */
void StringMessageCmd::execute(PuppetMasterClient * master)
{   
#ifndef DEDICATED_SERVER
    master->handleStringMessage((STRING_MESSAGE_TYPE)type_, msg_, pid1_);
#endif
}



//------------------------------------------------------------------------------
void StringMessageCmd::getNetworkOptions(PacketReliability & reliability,
                                PacketPriority    & priority,
                                unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
//     reliability = RELIABLE;
//     priority    = LOW_PRIORITY;
//     channel     = NC_CHAT;
}



//------------------------------------------------------------------------------
void StringMessageCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_STRING_MESSAGE_CMD);
  
    stream.Write(type_);

    network::writeToBitstream(stream, msg_);

    if (type_ == SMT_CHAT                  ||
        type_ == SMT_TEAM_CHAT             ||
        type_ == SMT_PLAYER_NAME)
    {
        stream.Write(pid1_);
    }
}


//------------------------------------------------------------------------------
void StringMessageCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    stream.Read(type_);
    network::readFromBitstream(stream, msg_);


    if (type_ == SMT_CHAT                  ||
        type_ == SMT_TEAM_CHAT             ||
        type_ == SMT_PLAYER_NAME)
    {
        stream.Read(pid1_);
    } else pid1_ = UNASSIGNED_SYSTEM_ADDRESS;
}

//********** CustomServerCmd **********//


//------------------------------------------------------------------------------
CustomServerCmd::CustomServerCmd()
{
}

//------------------------------------------------------------------------------
CustomServerCmd::CustomServerCmd(uint8_t type, RakNet::BitStream & data) :
    type_(type),
    args_stream_(data.GetData(), data.GetNumberOfBytesUsed(), true)
{
    
    s_log << Log::debug('n')
          << "CustomServerCmd constructed with type "
          << (unsigned)type
          << "\n";
}


//------------------------------------------------------------------------------
void CustomServerCmd::execute(PuppetMasterClient * master)
{
#ifndef DEDICATED_SERVER
    master->getGameLogic()->executeCustomCommand(type_, args_stream_);
    
    s_log << Log::debug('n')
          << "CustomServerCmd executed with type "
          << (unsigned)type_
          << "\n";
    
#endif
}


//------------------------------------------------------------------------------
void CustomServerCmd::getNetworkOptions(PacketReliability & reliability,
                                            PacketPriority    & priority,
                                            unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}

//------------------------------------------------------------------------------
void CustomServerCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_CUSTOM_SERVER_CMD);
    stream.Write(type_);

    network::writeToBitstream(stream, args_stream_);
}

//------------------------------------------------------------------------------
void CustomServerCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    stream.Read(type_);

    network::readFromBitstream(stream, args_stream_);
}




} // namespace network
