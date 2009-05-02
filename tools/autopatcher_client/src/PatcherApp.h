
#ifndef PATCHER_APP_INCLUDED
#define PATCHER_APP_INCLUDED


#include <fox/fx.h>


#include <raknet/RakPeerInterface.h>
#include <raknet/FileListTransferCBInterface.h>
#include <raknet/FileListTransfer.h>
#include <raknet/AutopatcherClient.h>


#include "VersionInfo.h"
#include "VersionHandshakePlugin.h"


namespace FX
{
class FXImage;
}

const std::string CONFIG_FILE = "config_autopatcher.xml";


//------------------------------------------------------------------------------
class PatcherApp : public FXApp, public FileListTransferCBInterface
{
    FXDECLARE(PatcherApp);
 public:
    enum
        {
            ID_HANDLE_NETWORK = FXApp::ID_LAST,
            ID_START_GAME,
            ID_PATCH
        };
    
    PatcherApp();
    virtual ~PatcherApp();


    virtual void init(int& argc,char** argv);
    

    virtual bool OnFile(OnFileStruct *onFileStruct);
    virtual void OnFileProgress(OnFileStruct * fs,
                                unsigned int count,
                                unsigned int total,
                                unsigned int length,
                                char *firstDataChunk);
    

    
    void createWidgets();
  
    long handleNetwork(FXObject* sender, FXSelector key, void* data);
    long startGame    (FXObject* sender, FXSelector key, void* data);
    long patch        (FXObject* sender, FXSelector key, void* data);

 protected:

    void onConnect();

    void onPatchingFinished();
    
    void onFailure(const char * info);

    void restartApp();

    void setIsPatching(bool b);

    void shutdownNetwork();

    network::ACCEPT_VERSION_CALLBACK_RESULT acceptVersionCallback(const VersionInfo & version);
    
    FX::FXImage * image_;
    FXLabel * status_label_;
    FXProgressBar * progress_total_;
    FXProgressBar * progress_file_;
    
    FXButton * start_game_button_;
    FXButton * patch_button_;

    FXMainWindow * main_window_;
    

    AutopatcherClient patcher_;
    FileListTransfer transfer_;
    RakPeerInterface * interface_;

    unsigned transfered_bytes_;

    bool is_patching_;
    bool patching_failed_;
    bool restart_needed_;
};

#endif
