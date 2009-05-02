
#ifndef TANK_GUITANKEQUIPMENT_INCLUDED
#define TANK_GUITANKEQUIPMENT_INCLUDED

#include <CEGUI/CEGUI.h>

#include "Score.h"
#include "RegisteredFpGroup.h"

class PuppetMasterClient;
class EquipmentListboxItem;

const unsigned NUM_PREVIEW_IMAGES = 5;

//------------------------------------------------------------------------------
class GUITankEquipment
{
 public:
    GUITankEquipment(PuppetMasterClient * puppet_master);
    virtual ~GUITankEquipment();
  
    void show(bool s);
    void toggleShow();

    void sendEquipmentSelection();

 protected:
    void loadWidgets();
    void fillComboboxes();

    void registerCallbacks();

    void showPreviewImage(CEGUI::Window * preview_image, const std::string & name);
    void setDescriptionText(EquipmentListboxItem * item);

    bool clickedCancelBtn(const CEGUI::EventArgs& e);
    bool clickedOkBtn(const CEGUI::EventArgs& e);
    bool onKeySelect(const CEGUI::EventArgs& e);
    bool comboboxListSelectionChanged(const CEGUI::EventArgs& e);
    bool comboboxListSelectionAccepted(const CEGUI::EventArgs& e);

    CEGUI::Window * root_equipment_;
    CEGUI::Window * equipment_window_;
    CEGUI::Window * desc_window_;
    CEGUI::ButtonBase * ok_btn_;
    CEGUI::ButtonBase * cancel_btn_;

    CEGUI::Combobox * equipment_combobox_[NUM_EQUIPMENT_SLOTS];

    CEGUI::Window * preview_images_[NUM_PREVIEW_IMAGES];

    std::vector<std::string> accepted_comboboxes_;

    PuppetMasterClient * puppet_master_;

    RegisteredFpGroup fp_group_;
};


#include "GUIOptions.h" ///< used for constants only
//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class EquipmentListboxItem : public CEGUI::ListboxTextItem
{
public:
    EquipmentListboxItem(const CEGUI::String& text, 
                         CEGUI::uint item_id,
                         bool available,
                         std::string description) : 
        ListboxTextItem(text, item_id),
        available_(available),
        description_(description)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);

        if(available_)
            setTextColours(LISTBOX_ITEM_TEXT_COLOR);
        else
            setTextColours(LISTBOX_ITEM_TEXT_DISABLED_COLOR);
    }

    bool available_;
    std::string description_;
};

#endif
