
#include "PatcherServer.h"

#include <raknet/MessageIdentifiers.h>
#include <raknet/RakNetworkFactory.h>



#include "NetworkUtils.h"
#include "RakAutoPacket.h"
#include "Log.h"
#include "Scheduler.h"
#include "ParameterManager.h"
#include "MessageIds.h"


namespace network
{

namespace patcher_server
{

//------------------------------------------------------------------------------
void ProgressLogger::OnAddFilesFromDirectoryStarted(FileList *fileList, char *dir)
{
    s_log << "started\n";
}

//------------------------------------------------------------------------------
void ProgressLogger::OnDirectory(FileList *fileList, char *dir, unsigned int directoriesRemaining)
{
    s_log << "dir: "
          << dir
          << "\n";
}

//------------------------------------------------------------------------------
void ProgressLogger::OnFile(FileList *fileList, char *dir, char *fileName, unsigned int fileSize)
{
    s_log << "file "
          << fileName
          << "\n";
}


//------------------------------------------------------------------------------
PatcherServer::PatcherServer() :
    ServerInterface(network::AcceptVersionCallbackServer(this, &PatcherServer::acceptVersionCallback))
{
    s_console.addFunction("initDb",
                          ConsoleFun(this, &PatcherServer::initializeDb),
                          &fp_group_);
    s_console.addFunction("updateDb",
                          ConsoleFun(this, &PatcherServer::updateDb),
                          &fp_group_);
}

//------------------------------------------------------------------------------
PatcherServer::~PatcherServer()
{
    // Don't disconnect, or server might crash on quit :-(
//    if (repository_.IsConnected()) repository_.Disconnect();
    
    interface_->DetachPlugin(&transfer_);
    interface_->DetachPlugin(&patcher_);
}


//------------------------------------------------------------------------------
void PatcherServer::start()
{
    // First, open our database connection.
    std::string host    = s_params.get<std::string>("db.host");
    std::string user    = s_params.get<std::string>("db.user");
    std::string passwd  = s_params.get<std::string>("db.passwd");
    std::string db_name = s_params.get<std::string>("db.name");
    unsigned db_port    = s_params.get<unsigned>   ("db.port");
    
    if (!repository_.Connect(host.c_str(), user.c_str(), passwd.c_str(), db_name.c_str(), db_port, NULL, 0))
    {
        Exception e;
        e << "Failed to open database "
          << db_name
          << " on "
          << host
          << " with user "
          << user;
        throw e;
    }

    // Then open the network interface, and put our plugins to work.
    ServerInterface::start("Patch Server",
                           s_params.get<unsigned>("network.listen_port"),
                           s_params.get<unsigned>("network.max_connections"),
                           s_params.get<unsigned>("network.sleep_timer"),
                           0.0f,
                           s_params.get<unsigned>("network.mtu_size"),
                           NULL);

    patcher_.SetAutopatcherRepositoryInterface(&repository_);
    patcher_.SetFileListTransferPlugin(&transfer_);

    interface_->AttachPlugin(&transfer_);
    interface_->AttachPlugin(&patcher_);    
}



//------------------------------------------------------------------------------
bool PatcherServer::handlePacket(Packet * packet)
{
    // we don't handle any packets ourselves, everything's done by the
    // plugins.
    return false;
}


//------------------------------------------------------------------------------
/**
 *  Accept all patcher clients, report same version as client has.
 */
network::ACCEPT_VERSION_CALLBACK_RESULT PatcherServer::acceptVersionCallback(const VersionInfo & version,
                                                                             VersionInfo & reported_version)
{
    reported_version = g_version;
    reported_version.major_ = version.major_;
    reported_version.minor_ = version.minor_;
    
    if (version.type_ != VERSION_PATCH_CLIENT.type_) return network::AVCR_TYPE_MISMATCH;
    else return network::AVCR_ACCEPT;
}


//------------------------------------------------------------------------------
std::string PatcherServer::initializeDb(const std::vector<std::string>&args)
{
    if (!repository_.DestroyAutopatcherTables())
    {
        s_log << Log::warning << repository_.GetLastError();
    }
    if (!repository_.CreateAutopatcherTables())  throw Exception(repository_.GetLastError());
    if (!repository_.AddApplication(s_params.get<std::string>("db.app_name").c_str(),
                                    s_params.get<std::string>("db.user").c_str()))
    {
        throw Exception(repository_.GetLastError());
    }
    
    return "done.";
}




//------------------------------------------------------------------------------
std::string PatcherServer::updateDb(const std::vector<std::string>&args)
{
    if (!repository_.UpdateApplicationFiles(s_params.get<std::string>("db.app_name").c_str(),
                                            s_params.get<std::string>("db.app_dir").c_str(),
                                            s_params.get<std::string>("db.user").c_str(),
                                            &logger_))
    {
        throw Exception(repository_.GetLastError());
    }

    return "done.";
}



}

}
