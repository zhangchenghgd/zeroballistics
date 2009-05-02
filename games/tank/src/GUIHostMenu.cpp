
#include "GUIHostMenu.h"

#include <boost/filesystem.hpp> ///< to get map names

#include "MainMenu.h"
#include "Utils.h"
#include "Paths.h"
#include "UserPreferences.h"

#include "Gui.h"

#include "ClassLoader.h"
#include "GameLogicServer.h"

//------------------------------------------------------------------------------
GUIHostMenu::GUIHostMenu(MainMenu * main_menu) :
    main_menu_(main_menu)
{
    enableFloatingPointExceptions(false);

    loadWidgets();

    fillComboboxes();

    registerCallbacks();

    loadValues();

    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
GUIHostMenu::~GUIHostMenu()
{
}

//------------------------------------------------------------------------------
void GUIHostMenu::show()
{
    hostmenu_window_->setVisible(true);
    hostmenu_window_->activate();

//    clickedHostBtn(CEGUI::EventArgs()); // development shortcut to game...
    
}

//------------------------------------------------------------------------------
void GUIHostMenu::hide()
{
    hostmenu_window_->setVisible(false);
}

//------------------------------------------------------------------------------
void GUIHostMenu::registerCallbacks()
{
    host_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIHostMenu::clickedHostBtn, this));
    cancel_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIHostMenu::clickedCancelBtn, this));

    hostmenu_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUIHostMenu::onKeyDownHostMenuWindow, this));
}

//------------------------------------------------------------------------------
void GUIHostMenu::loadWidgets()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("MainMenu_root/");

    if(!parent)
    {
        throw Exception("GUIHostMenu is missing the root window.");
    }
    
    // Use parent window name as prefix to avoid name clashes
    wm.loadWindowLayout("hostmenu.layout", parent->getName());
    hostmenu_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "hostmenu/window");

    server_name_editbox_ = (CEGUI::Editbox*)wm.getWindow(parent->getName() + "hostmenu/server_name_editbox");
    server_port_editbox_ = (CEGUI::Editbox*)wm.getWindow(parent->getName() + "hostmenu/server_port_editbox");
    game_type_combobox_ = (CEGUI::Combobox*)wm.getWindow(parent->getName() + "hostmenu/server_game_type_combobox");
    map_name_combobox_ = (CEGUI::Combobox*)wm.getWindow(parent->getName() + "hostmenu/server_map_combobox");
    max_players_combobox_ = (CEGUI::Combobox*)wm.getWindow(parent->getName() + "hostmenu/server_max_players_combobox");
    time_limit_combobox_ = (CEGUI::Combobox*)wm.getWindow(parent->getName() + "hostmenu/server_time_limit_combobox");

    host_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "hostmenu/host_btn");
    cancel_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "hostmenu/cancel_btn");

    // restrict editbox input
    server_name_editbox_->setValidationString("[0-9a-zA-Z\\.\\-\\ \\#]*");
    server_port_editbox_->setValidationString("[0-9]*");

    // add options to widget tree
    parent->addChildWindow(hostmenu_window_);

    hostmenu_window_->setVisible(false);
}

//------------------------------------------------------------------------------
void GUIHostMenu::fillComboboxes()
{
    // XXX Adding listbox items in layout is not supported by CEGUI yet.
    // fill resolution combobox
    CEGUI::ListboxTextItem * itm;

    // fill game type
    std::vector<std::string> game_types = s_server_logic_loader.getRegisteredClassNames();
    for(unsigned t=0; t < game_types.size(); t++)
    {
        std::string name = game_types[t].replace(0,std::string("GameLogicServer").length(),"");
        itm = new GameTextListboxItem(name, t, name);
        game_type_combobox_->addItem(itm);
    }

    // fill map combo
    using namespace boost::filesystem;
    std::vector<std::string> maps;

    // get map names
    try
    {
        path lvl_path(LEVEL_PATH);
        for (directory_iterator it(lvl_path);
             it != directory_iterator();
             ++it)
        {       
            maps.push_back(it->path().leaf());
        }
        
    } catch (basic_filesystem_error<path> & be)
    {
        s_log << Log::error << "Could not retrieve map names in GUIHostMenu\n";
    }

    for(unsigned t=0; t < maps.size(); t++)
    {
        itm = new GameTextListboxItem(maps[t], t, maps[t]);
        map_name_combobox_->addItem(itm);
    }

    // fill max players combo
    for(unsigned t=2; t < 9; t+=2)
    {
        itm = new GameTextListboxItem(toString(t), t, toString(t));
        max_players_combobox_->addItem(itm);
    }

    // fill time limit combo
    for(unsigned t=0; t < 6; t++)
    {
        itm = new GameTextListboxItem(toString((t+1)*5) + " min", t, toString((t+1)*5*60));
        time_limit_combobox_->addItem(itm);
    }
}

//------------------------------------------------------------------------------
bool GUIHostMenu::clickedHostBtn(const CEGUI::EventArgs& e)
{
    hostmenu_window_->setVisible(false);
    main_menu_->showMainMenu();

    if(saveValues())
    {
        main_menu_->hostServer();
    }
    else
    {
        main_menu_->showNotification("Invalid server configuration!\n");
    }

    return true;
}

//------------------------------------------------------------------------------
bool GUIHostMenu::clickedCancelBtn(const CEGUI::EventArgs& e)
{
    hide();
    main_menu_->showMainMenu();
    return true;
}

//------------------------------------------------------------------------------
bool GUIHostMenu::onKeyDownHostMenuWindow(const CEGUI::EventArgs& e)
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
bool GUIHostMenu::loadValues()
{
    // server name
    server_name_editbox_->setText(s_params.get<std::string>("server.settings.name"));

    // server port
    server_port_editbox_->setText(toString(s_params.get<unsigned>("server.settings.listen_port")));

    // game type
    std::string type = s_params.get<std::string>("server.settings.type");
    for(size_t item=0; item < game_type_combobox_->getItemCount(); ++item)
    {
        GameTextListboxItem * type_item = (GameTextListboxItem*)game_type_combobox_->getListboxItemFromIndex(item);
        if(type_item->data_ == type)
        {
            game_type_combobox_->setItemSelectState(item, true);
            game_type_combobox_->setText(game_type_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // map name
    std::string level_name = s_params.get<std::string>("server.settings.level_name");
    for(size_t item=0; item < map_name_combobox_->getItemCount(); ++item)
    {
        GameTextListboxItem * type_item = (GameTextListboxItem*)map_name_combobox_->getListboxItemFromIndex(item);
        if(type_item->data_ == level_name)
        {
            map_name_combobox_->setItemSelectState(item, true);
            map_name_combobox_->setText(map_name_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // max players
    unsigned max_players = s_params.get<unsigned>("server.settings.max_connections");
    for(size_t item=0; item < max_players_combobox_->getItemCount(); ++item)
    {
        GameTextListboxItem * type_item = (GameTextListboxItem*)max_players_combobox_->getListboxItemFromIndex(item);
        if(fromString<unsigned>(type_item->data_) == max_players)
        {
            max_players_combobox_->setItemSelectState(item, true);
            max_players_combobox_->setText(max_players_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // time limit
    float time_limit = s_params.get<float>("server.settings.time_limit");
    for(size_t item=0; item < time_limit_combobox_->getItemCount(); ++item)
    {
        GameTextListboxItem * type_item = (GameTextListboxItem*)time_limit_combobox_->getListboxItemFromIndex(item);
        if(fromString<float>(type_item->data_) == time_limit)
        {
            time_limit_combobox_->setItemSelectState(item, true);
            time_limit_combobox_->setText(time_limit_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    return true;
}

//------------------------------------------------------------------------------
bool GUIHostMenu::saveValues()
{

    // server name
    std::string sn = server_name_editbox_->getText().c_str();
    if(sn.length() > 0)
    {
        s_params.set<std::string>("server.settings.name", sn);
    }
    else
    {
        return false;
    }

    // port number
    std::string port_s = server_port_editbox_->getText().c_str();
    unsigned port = fromString<unsigned>(port_s);
    if(port > 0 && !port_s.empty())
    {
        s_params.set<unsigned>("server.settings.listen_port", port);
    }
    else
    {
        return false;
    }

    // game type
    CEGUI::ListboxItem * current_combo_editbox_item = NULL;
    current_combo_editbox_item = game_type_combobox_->findItemWithText(game_type_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        GameTextListboxItem * item = (GameTextListboxItem*)current_combo_editbox_item;
        s_params.set<std::string>("server.settings.type", item->data_);
    }
    else
    {
        return false;
    }

    // map name
    current_combo_editbox_item = map_name_combobox_->findItemWithText(map_name_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        GameTextListboxItem * item = (GameTextListboxItem*)current_combo_editbox_item;
        s_params.set<std::string>("server.settings.level_name", item->data_);
    }
    else
    {
        return false;
    }

    // max players
    current_combo_editbox_item = max_players_combobox_->findItemWithText(max_players_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        GameTextListboxItem * item = (GameTextListboxItem*)current_combo_editbox_item;
        s_params.set<unsigned>("server.settings.max_connections", fromString<unsigned>(item->data_));
    }
    else
    {
        return false;
    }

    // time limit
    current_combo_editbox_item = time_limit_combobox_->findItemWithText(time_limit_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        GameTextListboxItem * item = (GameTextListboxItem*)current_combo_editbox_item;
        s_params.set<float>("server.settings.time_limit", fromString<float>(item->data_));
    }
    else
    {
        return false;
    }

    // actually write values to user config file
    return s_params.saveParameters(getUserConfigFile(),CONFIG_SUPERSECTION);
}


