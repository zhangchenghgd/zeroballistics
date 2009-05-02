

#include "PatcherApp.h"

#include <fstream>


#include <fox/FXXPMImage.h>



#include <raknet/BitStream.h>
#include <raknet/StringCompressor.h>
#include <raknet/RakNetworkFactory.h>
#include <raknet/MessageIdentifiers.h>
#include <raknet/AutopatcherPatchContext.h>


#include "ParameterManager.h"
#include "MultipleConnectPlugin.h"
#include "NetworkUtils.h"
#include "MessageIds.h"
#include "RakAutoPacket.h"



/// in milliseconds
unsigned HANDLE_NETWORK_INTERVAL = 10;
unsigned SPLIT_MESSAGE_PROGRESS_INTERVAL = 1;


const char * LAST_PATCH_DATE_FILENAME = "last_patch_date.txt";

const char * WINDOW_TITLE = "Game Autopatcher";

const char * RESTART_FILE = "patcher_restart.txt";


#ifdef _WIN32
const char * APP_EXE_PARAM = "patcher.app_exe_win";
const char * APP_NAME_PARAM = "patcher.app_name_win";
#else
const char * APP_EXE_PARAM = "patcher.app_exe_linux";
const char * APP_NAME_PARAM = "patcher.app_name_linux";
#endif


const char * GENERAL_FAIL_MSG = "Failed to update to the latest version.";


const unsigned NUM_CONNECT_RETRIES = 2;

FXDEFMAP(PatcherApp) PatcherAppObjectMap[] = {
    FXMAPFUNC(SEL_TIMEOUT, PatcherApp::ID_HANDLE_NETWORK, PatcherApp::handleNetwork),
    FXMAPFUNC(SEL_COMMAND, PatcherApp::ID_START_GAME,     PatcherApp::startGame),
    FXMAPFUNC(SEL_COMMAND, PatcherApp::ID_PATCH,          PatcherApp::patch)
};




FXIMPLEMENT(PatcherApp, FXApp, PatcherAppObjectMap, ARRAYNUMBER(PatcherAppObjectMap));


//------------------------------------------------------------------------------
PatcherApp::PatcherApp() :
    FXApp("Full Metal Soccer","QuantiCode"),
    image_(NULL),
    status_label_(NULL),
    progress_total_(NULL),
    progress_file_(NULL),
    start_game_button_(NULL),
    patch_button_(NULL),
    main_window_(NULL),
    interface_(NULL),
    transfered_bytes_(0),
    is_patching_(false),
    patching_failed_(false)
{
    patcher_.SetFileListTransferPlugin(&transfer_);
}


//------------------------------------------------------------------------------
PatcherApp::~PatcherApp()
{
    shutdownNetwork();
    delete image_;
}


//------------------------------------------------------------------------------
void PatcherApp::init(int& argc,char** argv)
{
    FXApp::init(argc, argv);
        
    main_window_ = new FXMainWindow(this, WINDOW_TITLE,
                                    NULL,NULL,DECOR_ALL & ~(DECOR_RESIZE));

    FXFileStream stream;
    if (stream.open(s_params.get<std::string>("patcher.bg_image").c_str(), FXStreamLoad))
    {
        image_ = new FXGIFImage(this);
        if (image_->loadPixels(stream))
        {
            new FXImageFrame(main_window_, image_, LAYOUT_SIDE_TOP);
        } else
        {
            delete image_; image_=NULL;
        }
    }

    FXMatrix * matrix_frame = new FXMatrix(main_window_, 2, LAYOUT_SIDE_TOP | MATRIX_BY_ROWS | LAYOUT_FILL_X);

    const FXint LABEL_STYLE = LAYOUT_RIGHT | LAYOUT_CENTER_Y;
    new FXLabel(matrix_frame, "Total: ", NULL, LABEL_STYLE);
    new FXLabel(matrix_frame, "File: " , NULL, LABEL_STYLE);
    
    const FXint PROGRESS_STYLE = (FRAME_RAISED | FRAME_THICK |
                                  LAYOUT_FILL_X | LAYOUT_CENTER_Y |
                                  PROGRESSBAR_PERCENTAGE | LAYOUT_FILL_COLUMN);
    progress_total_ = new FXProgressBar(matrix_frame, NULL, 0, PROGRESS_STYLE);
    progress_file_  = new FXProgressBar(matrix_frame, NULL, 0, PROGRESS_STYLE);
    
    status_label_ = new FXLabel(main_window_, "Ready.", NULL, LAYOUT_FILL_X | LAYOUT_SIDE_TOP);
    
    
    // Button horizontal frame
    FXHorizontalFrame * button_frame = new FXHorizontalFrame(main_window_, LAYOUT_FILL_X | LAYOUT_SIDE_BOTTOM);
    const FXint BUTTON_STYLE = FRAME_RAISED|FRAME_THICK|LAYOUT_SIDE_BOTTOM|LAYOUT_FILL_X;
    
    start_game_button_ = new FXButton(button_frame,"&Start Game",
                                      NULL, this, PatcherApp::ID_START_GAME, BUTTON_STYLE);
    patch_button_      = new FXButton(button_frame,"&Check for latest version...",
                                      NULL, this, PatcherApp::ID_PATCH,      BUTTON_STYLE);
    new FXButton(button_frame, "&Quit", NULL, this, FXApp::ID_QUIT, BUTTON_STYLE);

    create();
    main_window_->show(PLACEMENT_SCREEN);
}



//------------------------------------------------------------------------------
bool PatcherApp::OnFile(OnFileStruct * fs)
{
    std::string msg;
    bool failure = false;
    bool fatal = false;
    
    switch (fs->context.op)
    {
    case PC_HASH_WITH_PATCH:
        msg = "Patched: ";
        break;
    case PC_WRITE_FILE:
        msg = "Written: ";
        break;
    case PC_ERROR_FILE_WRITE_FAILURE:
        msg = "Failed to write ";
        failure = true;
        break;
    case PC_ERROR_PATCH_TARGET_MISSING:
        msg = "Patch target missing: ";
        failure = true;
        break;
    case PC_ERROR_PATCH_APPLICATION_FAILURE:
        msg = "Patching failed for ";
        failure = true;
        fatal   = true;
        break;
    case PC_ERROR_PATCH_RESULT_CHECKSUM_FAILURE:
        msg = "Checksum incorrect for ";
        failure = true;
        break;
    case PC_NOTICE_WILL_COPY_ON_RESTART:
        msg = "Copy pending restart: ";
        break;
    case PC_NOTICE_FILE_DOWNLOADED:
        msg = "Downloaded: ";
        break;
    case PC_NOTICE_FILE_DOWNLOADED_PATCH:
        msg = "Downloaded Patch: ";
        break;
    default:
        msg = "Warning: Unknown file context: ";
    }
    msg += fs->fileName;

    bool stop_patching = false;
    if (failure)
    {
        patching_failed_ = true;
        
        if (fatal)
        {
            FXMessageBox::error(main_window_, MBOX_OK, "Error", msg.c_str());
            stop_patching = true;
        } else
        {
            if (FXMessageBox::error(main_window_, MBOX_YES_NO, "Error", (msg + "\n\nContinue patching?").c_str()) ==
                MBOX_CLICKED_NO)
            {
                stop_patching = true;
            }
        }
    }
    
    s_log << msg
          << " ("
          << (float)fs->compressedTransmissionLength / (1024.0f*1024.0f)
          << " MB)\n";
    
    if (stop_patching)
    {
        s_log << "aborting patch.\n";

        interface_->DetachPlugin(&transfer_);
        interface_->DetachPlugin(&patcher_);
        
        onFailure(GENERAL_FAIL_MSG);
    }
    
    // Return false for the file data to be deallocated automatically
    return false;
}


//------------------------------------------------------------------------------
void PatcherApp::OnFileProgress(OnFileStruct * fs,
                                unsigned int count,
                                unsigned int total,
                                unsigned int length,
                                char *firstDataChunk)
{
    transfered_bytes_ += length;
    
    progress_total_->setTotal   (fs->setTotalCompressedTransmissionLength);
    progress_total_->setProgress(transfered_bytes_);
    
    progress_file_->setTotal   (total);
    progress_file_->setProgress(count);
    
    status_label_->setText(FXString("Receiving ") + fs->fileName + "...");
}

//------------------------------------------------------------------------------
long PatcherApp::handleNetwork(FXObject* sender,FXSelector key,void* data)
{
    assert(interface_);
    
    network::RakAutoPacket packet(interface_);    
    while (packet.receive())
    {
        switch(packet->data[0])
        {
        case ID_CONNECTION_REQUEST_ACCEPTED:
            onConnect();
            break;
        case ID_CONNECTION_ATTEMPT_FAILED:
            onFailure("Failed to connect to server.");
            break;
        case ID_NO_FREE_INCOMING_CONNECTIONS:
            onFailure("The server is currently not available.\nPlease try again in a few minutes.");
            break;
        case ID_DISCONNECTION_NOTIFICATION:
            if (is_patching_) onFailure("Connection to server closed unexpectedly.");
            break;
        case ID_CONNECTION_LOST:
            onFailure("Connection to server lost.");
            break;
        case network::VHPI_VERSION_MISMATCH:
            onFailure("Patch server has mismatching version.");
            break;
        case network::VHPI_TYPE_MISMATCH:
            onFailure("Target server is not a valid patch server.");
            break;

        case ID_AUTOPATCHER_REPOSITORY_FATAL_ERROR:
        {
            char buff[256];
            RakNet::BitStream temp(packet->data, packet->length, false);
            temp.IgnoreBits(8);
            stringCompressor->DecodeString(buff, 256, &temp);
            onFailure((FXString("Patching failed:\n") + buff).text());
            break;
        }
            
        case ID_AUTOPATCHER_FINISHED:
            if (patching_failed_)
            {
                onFailure(GENERAL_FAIL_MSG);
            } else
            {
                onPatchingFinished();
            }
            break;
                
        case ID_AUTOPATCHER_RESTART_APPLICATION:
            restartApp();
            break;
        default:
            network::defaultPacketAction(packet, interface_);
        }
    }

    if (!is_patching_)
    {
        shutdownNetwork();
    } else
    {  
        // re-schedule this function
        addTimeout(this, ID_HANDLE_NETWORK, HANDLE_NETWORK_INTERVAL, NULL);
    }
    
    return 0;
}


//------------------------------------------------------------------------------
long PatcherApp::startGame(FXObject* sender, FXSelector key, void* data)
{
#ifdef _WIN32
    // launch game
    SHELLEXECUTEINFO SHInfo = {0};

    SHInfo.cbSize = sizeof (SHELLEXECUTEINFO);
    SHInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    SHInfo.lpVerb = "open";
    SHInfo.lpFile = s_params.get<std::string>(APP_EXE_PARAM).c_str();
    SHInfo.lpParameters = NULL;
    SHInfo.nShow = SW_SHOWNORMAL;

    if(!ShellExecuteEx(&SHInfo)) //execute cmd
    {
        char buf[256];
        DWORD dwErrorCode = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, 0);

        std::string err_msg = "Failed to launch game. Error: ";
        err_msg += buf;
        FXMessageBox::error(main_window_, MBOX_OK, "Error", err_msg.c_str());
    }
#else
    system(s_params.get<std::string>(APP_EXE_PARAM).c_str());
#endif
    
    return 1;
}

//------------------------------------------------------------------------------
long PatcherApp::patch(FXObject* sender, FXSelector key, void* data)
{
    assert (!is_patching_);
    
    setIsPatching(true);

    patching_failed_ = false;
    
    progress_total_->setProgress(0);
    progress_file_ ->setProgress(0);
    
    status_label_->setText("Connecting to server...");
    
    
    interface_ = RakNetworkFactory::GetRakPeerInterface();

    SocketDescriptor socketDescriptor(0,0);
    if (!interface_->Startup(1,0,&socketDescriptor, 1))
    {
        throw Exception("Failed to startup network interface.\n");
    }

    interface_->SetSplitMessageProgressInterval(SPLIT_MESSAGE_PROGRESS_INTERVAL);
    
    interface_->AttachPlugin(new network::VersionHandshakePlugin(
                                 network::AcceptVersionCallbackClient(this, &PatcherApp::acceptVersionCallback)));
    interface_->AttachPlugin(&patcher_);
    interface_->AttachPlugin(&transfer_);
    
    network::MultipleConnectPlugin * connect_plugin = new network::MultipleConnectPlugin();
    interface_->AttachPlugin(connect_plugin);
    connect_plugin->connect(s_params.get<std::vector<std::string> >("patcher.hosts"),
                            s_params.get<std::vector<unsigned> >   ("patcher.ports"),
                            NUM_CONNECT_RETRIES);

    handleNetwork(this, FXSEL(SEL_TIMEOUT, ID_HANDLE_NETWORK), NULL);
    
    return 1;
}


//------------------------------------------------------------------------------
void PatcherApp::onConnect()
{
    assert(interface_->NumberOfConnections() == 1);

    std::string app_dir = s_params.get<std::string>("patcher.app_dir");
    s_log << "Patching to "
          << app_dir
          << "\n";

    std::string last_date;
    std::ifstream ifstr(LAST_PATCH_DATE_FILENAME);
    if (ifstr) ifstr >> last_date;
        
    if (patcher_.PatchApplication(s_params.get<std::string>(APP_NAME_PARAM).c_str(),
                                  app_dir.c_str(),
                                  last_date.empty() ? NULL : last_date.c_str(),
                                  interface_->GetSystemAddressFromIndex(0),
                                  this, RESTART_FILE, getArgv()[0]))
    {
        FXString msg = "Retrieving changeset";
        if (!last_date.empty())
        {
            msg += " since ";
            msg += last_date.c_str();
        }
        msg += "...";
        status_label_->setText(msg);
    } else
    {
        onFailure("Internal error.");
    }
}


//------------------------------------------------------------------------------
void PatcherApp::onPatchingFinished()
{
    setIsPatching(false);

    progress_total_->setProgress(1);
    progress_total_->setTotal(1);
    progress_file_ ->setProgress(1);
    progress_file_ ->setTotal(1);
    
    std::string last_date = patcher_.GetServerDate();
    // strip the '\n' which is appended to the date...
    if (*last_date.rbegin() == '\n') last_date = last_date.substr(0, last_date.size()-1);

    std::ofstream ofstr(LAST_PATCH_DATE_FILENAME, std::ios_base::trunc);
    if (!ofstr)
    {
        status_label_->setText("Unable to write last patch date!");
    } else
    {
        ofstr << last_date;
        status_label_->setText("Updated to latest version.");
    }
}


//------------------------------------------------------------------------------
void PatcherApp::onFailure(const char * info)
{
    setIsPatching(false);
    
    status_label_->setText("Version update failed. Sorry.");
    FXMessageBox::error(main_window_, MBOX_OK, "Error", info);
}


//------------------------------------------------------------------------------
void PatcherApp::restartApp()
{
#ifdef _WIN32
    // launch patcher_restarter with RESTART_FILE
    SHELLEXECUTEINFO SHInfo = {0};

    SHInfo.cbSize = sizeof (SHELLEXECUTEINFO);
    SHInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    SHInfo.lpVerb = "open";
    SHInfo.lpFile = "patch_restarter.exe";
    SHInfo.lpParameters = RESTART_FILE;
    SHInfo.nShow = SW_SHOWNORMAL;

    if(!ShellExecuteEx(&SHInfo)) //execute cmd
    {
        char buf[256];
        DWORD dwErrorCode = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, 0);

        std::string err_msg = "Failed to launch patch restarter. Error: ";
        err_msg += buf;
        FXMessageBox::error(main_window_, MBOX_OK, "Error", err_msg.c_str());
    }    
#endif

    // quit this here
    this->handle(this, FXSEL(SEL_COMMAND, ID_QUIT), NULL);
}


//------------------------------------------------------------------------------
/**
 *  Don't shut down network here, as we might be handling a
 *  packet. Network will be shut down if is_patching_ is false.
 */
void PatcherApp::setIsPatching(bool b)
{
    if (b)
    {        
        patch_button_->disable();
        start_game_button_->disable();
        is_patching_ = true;
    } else
    {
        patch_button_->enable();
        start_game_button_->enable();
        is_patching_ = false;
    }
}


//------------------------------------------------------------------------------
void PatcherApp::shutdownNetwork()
{
    if (patcher_.IsPatching()) patcher_.CancelDownload();
    
    if (interface_)
    {
        if (interface_->NumberOfConnections())
        {
            assert(interface_->NumberOfConnections() == 1);
            interface_->CloseConnection(interface_->GetSystemAddressFromIndex(0), true);
        }
        interface_->DetachPlugin(&transfer_);
        interface_->DetachPlugin(&patcher_);
    
        interface_->Shutdown(500);
        RakNetworkFactory::DestroyRakPeerInterface(interface_);
        interface_ = NULL;
    }
}


//------------------------------------------------------------------------------
network::ACCEPT_VERSION_CALLBACK_RESULT PatcherApp::acceptVersionCallback(const VersionInfo & version)
{
    // accept only patch server,  same version
    VersionInfo cmp_version(toupper(g_version.type_), g_version.major_, g_version.minor_);

    if (cmp_version.type_ != version.type_) return network::AVCR_TYPE_MISMATCH;
    else return cmp_version == version ? network::AVCR_ACCEPT : network::AVCR_VERSION_MISMATCH;
}

