

#include <string>
#include <vector>


#include <raknet/FileListTransfer.h>
#include <raknet/AutopatcherMySQLRepository.h>
#include <raknet/AutopatcherServer.h>
#include <raknet/FileList.h>


#include "ServerInterface.h"


class RakPeerInterface;

class VersionInfo;
namespace network
{
    class VersionHandshakePlugin;
}


namespace network
{

namespace patcher_server
{

//------------------------------------------------------------------------------
class ProgressLogger : public FileListProgress
{
 public:
    virtual void OnAddFilesFromDirectoryStarted(FileList *fileList, char *dir);
    virtual void OnDirectory(FileList *fileList, char *dir, unsigned int directoriesRemaining);
    virtual void OnFile(FileList *fileList, char *dir, char *fileName, unsigned int fileSize);
};


//------------------------------------------------------------------------------
class PatcherServer : public ServerInterface
{
 public:
    PatcherServer();
    virtual ~PatcherServer();

    void start();
    
 protected:

    virtual bool handlePacket(Packet * packet);

    network::ACCEPT_VERSION_CALLBACK_RESULT acceptVersionCallback(const VersionInfo & version,
                                                                  VersionInfo & reported_version);
    
    
    std::string resetDb     (const std::vector<std::string>&args);
    std::string addApp      (const std::vector<std::string>&args);
    std::string updateDb    (const std::vector<std::string>&args);


    FileListTransfer transfer_;
    AutopatcherServer patcher_;
    AutopatcherMySQLRepository repository_;

    ProgressLogger logger_;
};

}

}
