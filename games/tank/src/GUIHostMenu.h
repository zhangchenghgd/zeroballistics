
#ifndef TANK_GUIHOSTMENU_INCLUDED
#define TANK_GUIHOSTMENU_INCLUDED

#include <CEGUI/CEGUI.h>

#include "ParameterManager.h"

class MainMenu;

//------------------------------------------------------------------------------
class GUIHostMenu
{
 public:
    GUIHostMenu(MainMenu * main_menu);
    virtual ~GUIHostMenu();
    
    void show();
    
 protected:

    void registerCallbacks();
    void loadWidgets();
    void fillComboboxes();

    bool clickedHostBtn(const CEGUI::EventArgs& e);
    bool clickedCancelBtn(const CEGUI::EventArgs& e);

    bool onKeyDownHostMenuWindow(const CEGUI::EventArgs& e);

    bool loadValues();
    bool saveValues();

    CEGUI::Window * hostmenu_window_;
    CEGUI::ButtonBase * cancel_btn_;
    CEGUI::ButtonBase * host_btn_;

    CEGUI::Editbox * server_name_editbox_;
    CEGUI::Editbox * server_port_editbox_;
    CEGUI::Combobox * game_type_combobox_;
    CEGUI::Combobox * map_name_combobox_;
    CEGUI::Combobox * max_players_combobox_;
    CEGUI::Combobox * time_limit_combobox_;

    MainMenu * main_menu_;
};

#include "GUIOptions.h" ///< used for constants only
//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class GameTextListboxItem : public CEGUI::ListboxTextItem
{
public:
    GameTextListboxItem(const CEGUI::String& text, CEGUI::uint item_id, std::string data) : 
        ListboxTextItem(text, item_id),
        data_(data)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    std::string data_;
};


#endif
