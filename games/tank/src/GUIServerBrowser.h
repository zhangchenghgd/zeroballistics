
#ifndef TANK_GUISERVERBROWSER_INCLUDED
#define TANK_GUISERVERBROWSER_INCLUDED

#include <CEGUI/CEGUI.h>

#include "ServerList.h"
#include "ServerInfo.h"

class MainMenu;

const unsigned INTERNET_LIST = 0;
const unsigned LAN_LIST = 1;

const unsigned NUM_SERVER_LISTS = 2;

//------------------------------------------------------------------------------
class GUIServerBrowser
{
 public:
    GUIServerBrowser(MainMenu * main_menu);
    virtual ~GUIServerBrowser();
    
    void show();
    void hide();

 protected:

    void registerCallbacks();
    void loadWidgets();
    void fillComboboxes();

    bool clickedInternetBtn(const CEGUI::EventArgs& e);
    bool clickedLanBtn(const CEGUI::EventArgs& e);
    bool clickedCancelBtn(const CEGUI::EventArgs& e);
    bool clickedConnectBtn(const CEGUI::EventArgs& e);
    bool clickedUpdateBtn(const CEGUI::EventArgs& e);
    bool clickedScanLanBtn(const CEGUI::EventArgs& e);
    bool clickedDirectConnectToBtn(const CEGUI::EventArgs& e);
    bool clickedDirectConnectOkBtn(const CEGUI::EventArgs& e);
    bool clickedDirectConnectCancelBtn(const CEGUI::EventArgs& e);

    bool onKeyDownServerBrowserWindow(const CEGUI::EventArgs& e);
    bool onKeyHostEditbox(const CEGUI::EventArgs& e);
    bool onKeyPortEditbox(const CEGUI::EventArgs& e);

    bool listSelectionChanged(const CEGUI::EventArgs& e);
    bool listDoubleClicked(const CEGUI::EventArgs& e);

    //-------------------- Observer callbacks --------------------
    void onInternetServerFound();
    void onLanServerFound();
    void onNewGameVersionAvailable();
    void onMasterUnreachable();

    void clearInfoWindow();
    void fillInfoWindow(const std::string & server_name,
                        const std::string & map_name,
                        const std::string & ip,
                        const std::string & port,
                        const std::string & players);

    void addServerListRow(CEGUI::MultiColumnList * list,
                          const network::master::ListServerInfo & info);

    CEGUI::Window * serverbrowser_window_;
    CEGUI::Window * internet_tab_window_;
    CEGUI::Window * lan_tab_window_;
    CEGUI::ButtonBase * internet_btn_;
    CEGUI::ButtonBase * lan_btn_;    
    CEGUI::ButtonBase * cancel_btn_;
    CEGUI::ButtonBase * connect_btn_;
    CEGUI::ButtonBase * update_btn_;  
    CEGUI::ButtonBase * scan_lan_btn_;  
    CEGUI::ButtonBase * direct_connect_to_btn_; 
    CEGUI::Window * notification_area_;

    CEGUI::MultiColumnList * gui_list_[NUM_SERVER_LISTS];
    CEGUI::Window * server_count_[NUM_SERVER_LISTS];

    CEGUI::Window * info_name_;
    CEGUI::Window * info_ip_;
    CEGUI::Window * info_port_;
    CEGUI::Window * info_map_;
    CEGUI::Window * info_players_;
    CEGUI::Window * info_minimap_;

    // direct connect to IP elements
    CEGUI::Window * direct_connect_window_;
    CEGUI::ButtonBase * direct_connect_ok_btn_;  
    CEGUI::ButtonBase * direct_connect_cancel_btn_;  
    CEGUI::Editbox * direct_connect_host_editbox_;
    CEGUI::Editbox * direct_connect_port_editbox_;

    MainMenu * main_menu_;

    std::auto_ptr<network::master::ServerList> server_list_[NUM_SERVER_LISTS];

    RegisteredFpGroup fp_group_;
};


#include "GUIOptions.h" ///< only used for constants below
//------------------------------------------------------------------------------
/*** 
 * Specialized sub-class for ListboxItemWindow that represents a  
 * Name/Value Element inside a listbox
 **/
class ServerListInfoElement : public CEGUI::ListboxTextItem
{
public:
    ServerListInfoElement(const std::string & text, const network::master::ServerInfo & info) :
        ListboxTextItem(text),
        info_(info)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont               (LISTBOX_ITEM_FONT_SMALL);
        setSelectionColours   (LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours        (LISTBOX_ITEM_TEXT_COLOR);
    }

    network::master::ServerInfo info_;
};

//------------------------------------------------------------------------------
/*** 
 * Specialized sub-class for ListboxItemWindow that represents a  
 * Name/Value Element inside a listbox
 **/
class ServerListTextElement : public CEGUI::ListboxTextItem
{
public:
    ServerListTextElement(const std::string & text) :
        ListboxTextItem(text)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont               (LISTBOX_ITEM_FONT_SMALL);
        setSelectionColours   (LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours        (LISTBOX_ITEM_TEXT_COLOR);
    }
};

//------------------------------------------------------------------------------
/*** 
 * Specialized sub-class for ListboxItemWindow that represents a  
 * Name/Value Element inside a listbox
 **/
class ServerListNumericElement : public CEGUI::ListboxTextItem
{
public:
    ServerListNumericElement(const std::string & text, unsigned num) :
        ListboxTextItem(text),
        num_(num)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont               (LISTBOX_ITEM_FONT_SMALL);
        setSelectionColours   (LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours        (LISTBOX_ITEM_TEXT_COLOR);
    }

    // overload operator for sorting
    bool operator < ( const ListboxItem& rhs ) const
    {
       const ServerListNumericElement * right = (const ServerListNumericElement*)(&rhs);
       return num_ < right->num_;
    }

    // overload operator for sorting
    bool operator > ( const ListboxItem& rhs ) const
    {
       const ServerListNumericElement * right = (const ServerListNumericElement*)(&rhs);
       return num_ > right->num_;
    }

    unsigned num_;
};

//------------------------------------------------------------------------------
/*** 
 * Specialized sub-class for ListboxItemWindow that represents a  
 * Name/Value Element inside a listbox
 **/
class ServerListPlayerElement : public CEGUI::ListboxTextItem
{
public:
    ServerListPlayerElement(const std::string & text, unsigned num_players, unsigned max_players) :
        ListboxTextItem(text),
        num_players_(num_players),
        max_players_(max_players)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont               (LISTBOX_ITEM_FONT_SMALL);
        setSelectionColours   (LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours        (LISTBOX_ITEM_TEXT_COLOR);
    }

    // overload operator for sorting
    bool operator < ( const ListboxItem& rhs ) const
    {
        const ServerListPlayerElement * right = (const ServerListPlayerElement*)(&rhs);

        if(max_players_ == right->max_players_)
        {
            return num_players_ < right->num_players_;
        }
        else
        {
            return max_players_ < right->max_players_;
        }
    }

    // overload operator for sorting
    bool operator > ( const ListboxItem& rhs ) const
    {
        const ServerListPlayerElement * right = (const ServerListPlayerElement*)(&rhs);

        if(max_players_ == right->max_players_)
        {
            return num_players_ > right->num_players_;
        }
        else
        {
            return max_players_ > right->max_players_;
        }
    }


    unsigned num_players_;
    unsigned max_players_;
};

#endif
