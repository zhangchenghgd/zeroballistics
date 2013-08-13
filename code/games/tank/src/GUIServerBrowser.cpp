
#include "GUIServerBrowser.h"

#include "MainMenu.h"

#include "Gui.h"
#include "ParameterManager.h"
#include "UserPreferences.h"
#include "Paths.h"


#include "NetworkCommand.h" // for interface enumeration

//------------------------------------------------------------------------------
GUIServerBrowser::GUIServerBrowser(MainMenu * main_menu) :
    main_menu_(main_menu)
{
    enableFloatingPointExceptions(false);

    loadWidgets();

    // setup gui lists with headers and create server lists
    for(unsigned c=0; c < NUM_SERVER_LISTS; c++)
    {
        server_list_[c].reset(new network::master::ServerList());

        // set init values for multicolumn lists
        gui_list_[c]->setSelectionMode(CEGUI::MultiColumnList::RowSingle);
        gui_list_[c]->setUserSortControlEnabled(true);
        gui_list_[c]->setUserColumnSizingEnabled(false);
        gui_list_[c]->setUserColumnDraggingEnabled(false);
        gui_list_[c]->setWantsMultiClickEvents(true);
        gui_list_[c]->setSortDirection(CEGUI::ListHeaderSegment::None);
        gui_list_[c]->setShowHorzScrollbar(false);
        gui_list_[c]->addColumn("Name",     0, CEGUI::UDim(0.48f, 0));
        gui_list_[c]->addColumn("Map",      1, CEGUI::UDim(0.20f, 0));
        gui_list_[c]->addColumn("Mode",     2, CEGUI::UDim(0.17f, 0));
        gui_list_[c]->addColumn("Players",  3, CEGUI::UDim(0.14f,0));
        
        // customize lists header
        gui_list_[c]->getListHeader()->getSegmentFromColumn(0).setProperty("Font","main_menu_font");
        gui_list_[c]->getListHeader()->getSegmentFromColumn(1).setProperty("Font","main_menu_font");
        gui_list_[c]->getListHeader()->getSegmentFromColumn(2).setProperty("Font","main_menu_font");
        gui_list_[c]->getListHeader()->getSegmentFromColumn(3).setProperty("Font","main_menu_font");                
    }        

    // make first tab visible on default
    serverbrowser_window_->setVisible(false);
    direct_connect_window_->setVisible(false);
    internet_tab_window_->setVisible(true);
    lan_tab_window_->setVisible(false);
    server_count_[LAN_LIST]->setVisible(false);

    registerCallbacks();

    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
GUIServerBrowser::~GUIServerBrowser()
{
}

//------------------------------------------------------------------------------
void GUIServerBrowser::show()
{
    serverbrowser_window_->setVisible(true);
    serverbrowser_window_->activate();
    direct_connect_window_->setVisible(false);


    ///< XXXX auto update, only for testing purpose
    clickedUpdateBtn(CEGUI::EventArgs());
    clickedScanLanBtn(CEGUI::EventArgs());
}

//------------------------------------------------------------------------------
void GUIServerBrowser::hide()
{
    serverbrowser_window_->setVisible(false); 
    direct_connect_window_->setVisible(false);

    for (unsigned i=0; i<NUM_SERVER_LISTS; ++i)
    {
        server_list_[i]->cancelQueries();
    }
}

//------------------------------------------------------------------------------
void GUIServerBrowser::registerCallbacks()
{
    internet_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedInternetBtn, this));
    lan_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedLanBtn, this));
    connect_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedConnectBtn, this));
    cancel_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedCancelBtn, this));
    serverbrowser_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUIServerBrowser::onKeyDownServerBrowserWindow, this));

    update_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedUpdateBtn, this));
    scan_lan_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedScanLanBtn, this));
    direct_connect_to_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedDirectConnectToBtn, this));

    // register all callbacks for gui multicolumn lists
    for(unsigned c=0; c < NUM_SERVER_LISTS; c++)
    {
        // list item selection
        gui_list_[c]->subscribeEvent(CEGUI::MultiColumnList::EventSelectionChanged, CEGUI::Event::Subscriber(&GUIServerBrowser::listSelectionChanged, this));
        gui_list_[c]->subscribeEvent(CEGUI::MultiColumnList::EventMouseDoubleClick, CEGUI::Event::Subscriber(&GUIServerBrowser::listDoubleClicked, this));
    }

    // direct connect to window
    direct_connect_ok_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedDirectConnectOkBtn, this));
    direct_connect_cancel_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIServerBrowser::clickedDirectConnectCancelBtn, this));
    direct_connect_host_editbox_->subscribeEvent(CEGUI::Editbox::EventKeyDown, CEGUI::Event::Subscriber(&GUIServerBrowser::onKeyHostEditbox, this));
    direct_connect_port_editbox_->subscribeEvent(CEGUI::Editbox::EventKeyDown, CEGUI::Event::Subscriber(&GUIServerBrowser::onKeyPortEditbox, this));

    // add Observer event for internet server list
    server_list_[INTERNET_LIST]->addObserver(
                            ObserverCallbackFun0(this, &GUIServerBrowser::onInternetServerFound),
                            network::master::SLE_FOUND_SERVER,
                            &fp_group_);

    // add Observer event for internet server list activated on new version
    server_list_[INTERNET_LIST]->addObserver(
                            ObserverCallbackFunUserData(this, &GUIServerBrowser::onServerMessage),
                            network::master::SLE_SERVER_MESSAGE,
                            &fp_group_);

    server_list_[INTERNET_LIST]->addObserver(
                            ObserverCallbackFun0(this, &GUIServerBrowser::onMasterUnreachable),
                            network::master::SLE_MASTER_SERVER_UNREACHABLE,
                            &fp_group_);

    // add Observer event for lan server list
    server_list_[LAN_LIST]->addObserver(
                            ObserverCallbackFun0(this, &GUIServerBrowser::onLanServerFound),
                            network::master::SLE_FOUND_SERVER,
                            &fp_group_);
}

//------------------------------------------------------------------------------
void GUIServerBrowser::loadWidgets()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("MainMenu_root/");

    if(!parent)
    {
        throw Exception("GUIServerBrowser is missing the root window.");
    }

    // Use parent window name as prefix to avoid name clashes
    wm.loadWindowLayout("serverbrowser.layout", parent->getName());
    wm.loadWindowLayout("directconnect.layout", parent->getName());

    serverbrowser_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/window");
    internet_tab_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/internet_container");
    lan_tab_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/lan_container");
    internet_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "serverbrowser/internet_btn");
    lan_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "serverbrowser/lan_btn");
    connect_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "serverbrowser/connect_btn");
    cancel_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "serverbrowser/cancel_btn");
    update_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "serverbrowser/internet/update_btn");
    scan_lan_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "serverbrowser/lan/scan_btn");
    direct_connect_to_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "serverbrowser/connect_to_ip_btn");
    gui_list_[INTERNET_LIST] = (CEGUI::MultiColumnList*) wm.getWindow(parent->getName() + "serverbrowser/internet/list");
    gui_list_[LAN_LIST] = (CEGUI::MultiColumnList*) wm.getWindow(parent->getName() + "serverbrowser/lan/list");
    server_count_[INTERNET_LIST] = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/filter_window/server_count_internet_text");
    server_count_[LAN_LIST] = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/filter_window/server_count_lan_text");
    notification_area_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/filter_window/notification_area_label");

    // server info text elements
    info_name_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/server_info/name_text");
    info_ip_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/server_info/ip_text");
    info_port_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/server_info/port_text");
    info_map_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/server_info/map_text");
    info_players_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/server_info/players_text");
    info_minimap_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "serverbrowser/server_info/minimap_image");

    // direct connect to IP elements
    direct_connect_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "direct_connect/window");
    direct_connect_ok_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "direct_connect/ok_btn");
    direct_connect_cancel_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "direct_connect/cancel_btn");
    direct_connect_host_editbox_ = (CEGUI::Editbox*) wm.getWindow(parent->getName() + "direct_connect/host_editbox");
    direct_connect_port_editbox_ = (CEGUI::Editbox*) wm.getWindow(parent->getName() + "direct_connect/port_editbox");


    // restrict editbox input
    direct_connect_host_editbox_->setValidationString("[0-9a-zA-Z.]*");
    direct_connect_port_editbox_->setValidationString("[0-9]*");

    // prefill direct connect editbox
    direct_connect_host_editbox_->setText(toString(s_params.get<std::string>("client.network.host")));
    direct_connect_port_editbox_->setText(toString(s_params.get<unsigned>("client.network.port")));


    if (!serverbrowser_window_ || !direct_connect_window_)
    {
        throw Exception("GUIServerBrowser screen is missing a widget!");
    }



    // add options to widget tree
    parent->addChildWindow(serverbrowser_window_);
    parent->addChildWindow(direct_connect_window_);
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedInternetBtn(const CEGUI::EventArgs& e)
{   
    internet_tab_window_->setVisible(true);
    lan_tab_window_->setVisible(false);
    server_count_[LAN_LIST]->setVisible(false);
    server_count_[INTERNET_LIST]->setVisible(true);

    // update info window according to current selection
    listSelectionChanged(CEGUI::WindowEventArgs(gui_list_[INTERNET_LIST]));

    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedLanBtn(const CEGUI::EventArgs& e)
{
    enableFloatingPointExceptions(false);
    notification_area_->setText("");
    enableFloatingPointExceptions(true);
    
    internet_tab_window_->setVisible(false);
    lan_tab_window_->setVisible(true);
    server_count_[LAN_LIST]->setVisible(true);
    server_count_[INTERNET_LIST]->setVisible(false);

    // update info window according to current selection
    listSelectionChanged(CEGUI::WindowEventArgs(gui_list_[LAN_LIST]));

    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedCancelBtn(const CEGUI::EventArgs& e)
{
    hide();
    main_menu_->showMainMenu();
    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedConnectBtn(const CEGUI::EventArgs& e)
{
    network::master::ServerInfo info;
    CEGUI::ListboxTextItem * cur_element = NULL;
    SystemAddress host_addr = UNASSIGNED_SYSTEM_ADDRESS;
    CEGUI::MultiColumnList * current_list = NULL;

    // handle internet tab
    if(internet_tab_window_->isVisible())
    {
        current_list = gui_list_[INTERNET_LIST];
    }
    else // handle LAN tab
    {
        current_list = gui_list_[LAN_LIST];
    }


    // get ServerListInfoElement of selected row
    cur_element  = (CEGUI::ListboxTextItem*)(current_list->getFirstSelectedItem());
    if (!cur_element) return true;

   
    unsigned current_row = current_list->getItemRowIndex(cur_element);
    ServerListInfoElement * info_element =
        dynamic_cast<ServerListInfoElement*>(current_list->getItemAtGridReference(
                                                 CEGUI::MCLGridRef(current_row,0)));
    if (!info_element) return true;


    hide();

    main_menu_->connectPunch(info_element->info_.guid_,
                             info_element->info_.address_,
                             info_element->info_.internal_port_);

//    // Check whether we are in the same subnet. If so, connect
//    // directly, else use nat-punchthrough.
//    std::vector<std::string> interfaces = network::enumerateInterfaces();
//    if (interfaces.empty() ||
//        !network::isSameSubnet(interfaces[0], info_element->info_.address_.ToString(false)))
//    {
//        main_menu_->connectPunch(info_element->info_.address_,
//                                 info_element->info_.internal_port_);
//    } else
//    {
//        main_menu_->connect(info_element->info_.address_.ToString(false),
//                            info_element->info_.address_.port);
//    }

    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedUpdateBtn(const CEGUI::EventArgs& e)
{
    clearInfoWindow();

    enableFloatingPointExceptions(false);
    notification_area_->setText("");
    enableFloatingPointExceptions(true);
    
    gui_list_[INTERNET_LIST]->resetList();
    server_list_[INTERNET_LIST]->reset();
    server_count_[INTERNET_LIST]->setText("-");

    server_list_[INTERNET_LIST]->queryMasterServer();
    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedScanLanBtn(const CEGUI::EventArgs& e)
{
    clearInfoWindow();

    gui_list_[LAN_LIST]->resetList();
    server_list_[LAN_LIST]->reset();
    server_count_[LAN_LIST]->setText("-");
    
    unsigned port = s_params.get<unsigned>("server.settings.listen_port");
    server_list_[LAN_LIST]->queryLan(port);

    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedDirectConnectToBtn(const CEGUI::EventArgs& e)
{
    hide();
    direct_connect_window_->setVisible(true);

    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedDirectConnectOkBtn(const CEGUI::EventArgs& e)
{    
    /// avoid calling this method twice in row
    if(!direct_connect_window_->isVisible()) return true;

    std::string host = direct_connect_host_editbox_->getText().c_str();
    unsigned port = fromString<unsigned>(std::string(direct_connect_port_editbox_->getText().c_str()));

    // save entered hostname
    try
    {
        s_params.set("client.network.host",host);
        s_params.set("client.network.port",port);
        s_params.saveParameters(getUserConfigFile(), CONFIG_SUPERSECTION);
    }
    catch(Exception & e)
    {
        s_log << Log::error << " occurred on saving host name at GUIServerBrowser::clickedDirectConnectOkBtn\n";
    }

    hide();
    main_menu_->connect(host, port);

    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::clickedDirectConnectCancelBtn(const CEGUI::EventArgs& e)
{
    show();
    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::onKeyDownServerBrowserWindow(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
    case CEGUI::Key::Escape: // exit from options on Escape Key
            clickedCancelBtn(e);
            return true;
    default:
        return false;
    }

    return false;
}


//------------------------------------------------------------------------------
bool GUIServerBrowser::onKeyHostEditbox(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
        case CEGUI::Key::Return:  
            clickedDirectConnectOkBtn(e);
            return true;
        case CEGUI::Key::Tab: 
            direct_connect_port_editbox_->activate();
            return true;
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::onKeyPortEditbox(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
        case CEGUI::Key::Return:  
            clickedDirectConnectOkBtn(e);
            return true;
        case CEGUI::Key::Tab: 
            direct_connect_host_editbox_->activate();
            return true;
        default:
            return false;
    }
}


//------------------------------------------------------------------------------
bool GUIServerBrowser::listSelectionChanged(const CEGUI::EventArgs& e)
{
    CEGUI::ListboxTextItem * cur_element = NULL;

	const CEGUI::WindowEventArgs &window_event_args = static_cast<const CEGUI::WindowEventArgs&>(e);

    CEGUI::MultiColumnList * list = static_cast<CEGUI::MultiColumnList *>(window_event_args.window);

    // get ServerListInfoElement of selected row
    cur_element  = (CEGUI::ListboxTextItem*)(list->getFirstSelectedItem());
    if(cur_element)
    {
        unsigned current_row = list->getItemRowIndex(cur_element);

        ServerListInfoElement * info_element = NULL;
        info_element = dynamic_cast<ServerListInfoElement*>(list->getItemAtGridReference(
                                                            CEGUI::MCLGridRef(current_row,0)));

        if(info_element)
        {
            network::master::ServerInfo info;
            info = info_element->info_;

            // fill info window
            fillInfoWindow(info.name_,
                           info.level_name_,
                           info.address_.ToString(false),
                           toString(info.address_.port),
                           toString(info.num_players_) + " / " + toString(info.max_players_));

        }
    }
    else // if no element is selected clear info window
    {
        clearInfoWindow();
    }

    return true;
}

//------------------------------------------------------------------------------
bool GUIServerBrowser::listDoubleClicked(const CEGUI::EventArgs& e)
{
    return clickedConnectBtn(e);
}

//------------------------------------------------------------------------------
void GUIServerBrowser::onInternetServerFound()
{
    enableFloatingPointExceptions(false);

    // handle only if server browser is visible
    if(!serverbrowser_window_->isVisible()) return;

    std::vector<network::master::ListServerInfo> server_info_list;
    server_info_list = server_list_[INTERNET_LIST]->getInfoList();

    for(unsigned c=gui_list_[INTERNET_LIST]->getRowCount(); c < server_info_list.size(); c++)
    {
        addServerListRow(gui_list_[INTERNET_LIST],
                         server_info_list[c]);
    }

    gui_list_[INTERNET_LIST]->handleUpdatedItemData();

    // update server count
    server_count_[INTERNET_LIST]->setText(toString(gui_list_[INTERNET_LIST]->getRowCount()));

    enableFloatingPointExceptions(true);
}

//------------------------------------------------------------------------------
void GUIServerBrowser::onLanServerFound()
{
    enableFloatingPointExceptions(false);

    // handle only if server browser is visible
    if(!serverbrowser_window_->isVisible()) return;

    std::vector<network::master::ListServerInfo> server_info_list;
    server_info_list = server_list_[LAN_LIST]->getInfoList();

    for(unsigned c=gui_list_[LAN_LIST]->getRowCount(); c < server_info_list.size(); c++)
    {
        addServerListRow(gui_list_[LAN_LIST],
                         server_info_list[c]);
    }

    gui_list_[LAN_LIST]->handleUpdatedItemData();

    // update server count
    server_count_[LAN_LIST]->setText(toString(gui_list_[LAN_LIST]->getRowCount()));

    enableFloatingPointExceptions(true);
}

//------------------------------------------------------------------------------
void GUIServerBrowser::onServerMessage(Observable*, void*m, unsigned)
{
    enableFloatingPointExceptions(false);

    const std::string & msg = *((const std::string*)m);

    notification_area_->setText(msg.c_str());

    enableFloatingPointExceptions(true);
}


//------------------------------------------------------------------------------
void GUIServerBrowser::onMasterUnreachable()
{
    enableFloatingPointExceptions(false);

    notification_area_->setText("Sorry, the master server currently is unreachable.");

    enableFloatingPointExceptions(true);
}


//------------------------------------------------------------------------------
void GUIServerBrowser::clearInfoWindow()
{
    std::string clr_text = "-";

    info_name_->setText(clr_text);
    info_ip_->setText(clr_text);
    info_port_->setText(clr_text);
    info_map_->setText(clr_text);
    info_players_->setText(clr_text); 
    info_minimap_->setProperty("Image", CEGUI::PropertyHelper::imageToString(NULL));
}

//------------------------------------------------------------------------------
void GUIServerBrowser::fillInfoWindow(  const std::string & server_name,
                                        const std::string & map_name,
                                        const std::string & ip,
                                        const std::string & port,
                                        const std::string & players)
{
    enableFloatingPointExceptions(false);

    info_name_->setText(server_name);
    info_ip_->setText(ip);
    info_port_->setText(port);
    info_map_->setText(map_name);
    info_players_->setText(players); 

    // show minimap preview if possible
    try
    {
        std::string full_minimap_name = LEVEL_PATH + map_name + "/minimap.png";

        CEGUI::Imageset * imageset = NULL;

         // if image has been loaded, take it from imageset otherwise generate imageset
        if(CEGUI::ImagesetManager::getSingleton().isImagesetPresent(full_minimap_name) == false)
        {
            CEGUI::Texture * minimap = CEGUI::System::getSingleton().getRenderer()->createTexture(full_minimap_name, "");
            imageset = CEGUI::ImagesetManager::getSingleton().createImageset(full_minimap_name, minimap);
            imageset->defineImage( full_minimap_name, 
                                   CEGUI::Point(0.0f, 0.0f), 
                                   CEGUI::Size(minimap->getWidth(), minimap->getHeight()), 
                                   CEGUI::Point(0.0f,0.0f));
        }
        else
        {
            imageset = CEGUI::ImagesetManager::getSingleton().getImageset(full_minimap_name);
        }

        CEGUI::String img = CEGUI::PropertyHelper::imageToString(&imageset->getImage(full_minimap_name));
        info_minimap_->setProperty("Image", img);

        enableFloatingPointExceptions(true);
        
    }
    catch(CEGUI::Exception e)
    {
        enableFloatingPointExceptions(true);
        
        s_log << Log::warning << "Unable to load minimap preview in GUIServerBrowser. Exception:" 
              << e.getMessage().c_str() << "\n";

        info_minimap_->setProperty("Image", CEGUI::PropertyHelper::imageToString(NULL));
    }
}

//------------------------------------------------------------------------------
void GUIServerBrowser::addServerListRow(CEGUI::MultiColumnList * list,
                                        const network::master::ListServerInfo & info)
{
    std::string players = toString(info.num_players_) + " / " + toString(info.max_players_);

    /// XXX testing insertion code
    unsigned row = list->addRow();
	list->setItem(new ServerListInfoElement(info.name_, info), 0, row);
	list->setItem(new ServerListTextElement(info.level_name_), 1, row);
	list->setItem(new ServerListTextElement(info.game_mode_), 2, row); 
	list->setItem(new ServerListPlayerElement(players, info.num_players_, info.max_players_), 3, row);
}

