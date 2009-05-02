
#include "GUITankEquipment.h"

#include "Utils.h"
#include "Player.h"
#include "PuppetMasterClient.h"
#include "ParameterManager.h"
#include "SdlApp.h"
#include "InputHandler.h"
#include "Gui.h"
#include "Paths.h"

#include "GameLogicClientCommon.h"
#include "UserPreferences.h"

#include "TankEquipment.h"

//------------------------------------------------------------------------------
GUITankEquipment::GUITankEquipment(PuppetMasterClient * puppet_master) :
            puppet_master_(puppet_master)
{
    enableFloatingPointExceptions(false);

    accepted_comboboxes_.resize(NUM_EQUIPMENT_SLOTS);

    loadWidgets();

    fillComboboxes();

    registerCallbacks();

    enableFloatingPointExceptions();


    s_input_handler.registerInputCallback("Show Tank Equipment Screen",
                                          input_handler::SingleInputHandler(this, &GUITankEquipment::toggleShow),
                                          &fp_group_);
}

//------------------------------------------------------------------------------
GUITankEquipment::~GUITankEquipment()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();
    wm.destroyWindow(root_equipment_);
}

//------------------------------------------------------------------------------
void GUITankEquipment::show(bool s)
{
    equipment_window_->setVisible(s);

    if(s)
    {
        cancel_btn_->activate();
        equipment_window_->activate();
    }
    else
    {
        equipment_window_->deactivate();
    }
}


//------------------------------------------------------------------------------
void GUITankEquipment::toggleShow()
{
    show(!equipment_window_->isVisible());
}

//------------------------------------------------------------------------------
void GUITankEquipment::sendEquipmentSelection()
{
    clickedOkBtn(CEGUI::EventArgs());
    show(false);
}

//------------------------------------------------------------------------------
void GUITankEquipment::loadWidgets()
{

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("TankApp_root/");

    if(!parent)
    {
        throw Exception("GUITankEquipment is missing the root window. XXX hack!");
    }
    
    // Use parent window name as prefix to avoid name clashes
    root_equipment_ = wm.loadWindowLayout("tankequipment.layout", parent->getName());
    equipment_window_ = (CEGUI::Window*)wm.getWindow(parent->getName() + "tankequipment/window");
    desc_window_ = (CEGUI::Window*)wm.getWindow(parent->getName() + "tankequipment/desc_text");

    // load comboboxes
    for(unsigned i=0; i < NUM_EQUIPMENT_SLOTS; i++)
    {
        equipment_combobox_[i]  = (CEGUI::Combobox*)wm.getWindow(parent->getName() + "tankequipment/equipment_combobox" + toString(i));
    }

    // load preview images
    for(unsigned i=0; i < NUM_PREVIEW_IMAGES; i++)
    {
        preview_images_[i] = (CEGUI::Window*)wm.getWindow(parent->getName() + "tankequipment/preview_image" + toString(i));
    }

    ok_btn_ = (CEGUI::ButtonBase*)wm.getWindow(parent->getName() + "tankequipment/ok_btn");
    cancel_btn_ = (CEGUI::ButtonBase*)wm.getWindow(parent->getName() + "tankequipment/cancel_btn");

    // add windows to widget tree
    parent->addChildWindow(equipment_window_);

    equipment_window_->setVisible(false);
}

//------------------------------------------------------------------------------
void GUITankEquipment::fillComboboxes()
{
    CEGUI::ListboxTextItem * itm;

    ///----------- Primary Weapons -----------------
    itm = new EquipmentListboxItem("Splash Cannon", PW_SPLASH, true,
                                   s_params.get<std::string>("splash_cannon.description"));
    equipment_combobox_[0]->addItem(itm);

    itm = new EquipmentListboxItem("Armor Piercing", PW_ARMOR_PIERCING, true, 
                                   s_params.get<std::string>("armor_piercing_cannon.description"));
    equipment_combobox_[0]->addItem(itm);

    itm = new EquipmentListboxItem("Heavy Impact", PW_HEAVY_IMPACT, true, 
                                   s_params.get<std::string>("heavy_impact_cannon.description"));
    equipment_combobox_[0]->addItem(itm);

    /// ----------------- Secondary Weapons -----------------
    itm = new EquipmentListboxItem("Machine Gun", SW_MACHINEGUN, true,
                                   s_params.get<std::string>("mg.description"));
    equipment_combobox_[1]->addItem(itm);

    itm = new EquipmentListboxItem("Flamethrower", SW_FLAME_THROWER, true,
                                   s_params.get<std::string>("flamethrower.description"));
    equipment_combobox_[1]->addItem(itm);

    itm = new EquipmentListboxItem("Laser", SW_LASER, true,
                                   s_params.get<std::string>("laser.description"));
    equipment_combobox_[1]->addItem(itm);

    /// ----------------- First Skills -----------------
    itm = new EquipmentListboxItem("Mines", FS_MINE, true,
                                   s_params.get<std::string>("mine.description"));
    equipment_combobox_[2]->addItem(itm);

    itm = new EquipmentListboxItem("Ram Bucket", FS_RAM, true,
                                   s_params.get<std::string>("skill.ram.description"));
    equipment_combobox_[2]->addItem(itm);

    itm = new EquipmentListboxItem("Guided Missile", FS_MISSILE, true,
                                   s_params.get<std::string>("missile.description"));
    equipment_combobox_[2]->addItem(itm);

    /// ----------------- Second Skills -----------------
    itm = new EquipmentListboxItem("Nano bots", SS_NANO_BOTS, true,
                                   s_params.get<std::string>("skill.nano_bots.description"));
    equipment_combobox_[3]->addItem(itm);

    itm = new EquipmentListboxItem("Shield", SS_SHIELD, true,
                                   s_params.get<std::string>("skill.shield.description"));
    equipment_combobox_[3]->addItem(itm);

    itm = new EquipmentListboxItem("Boost", SS_BOOST, true,
                                   s_params.get<std::string>("skill.boost.description"));
    equipment_combobox_[3]->addItem(itm);


    accepted_comboboxes_ = s_params.get<std::vector<std::string> >("player.equipment");
    assert(accepted_comboboxes_.size() == 4);

    /// select equipment as preselected in params
    for(unsigned i=0; i < NUM_EQUIPMENT_SLOTS; i++)
    {
        for(size_t item = 0; item < equipment_combobox_[i]->getItemCount(); item++)
        {
            EquipmentListboxItem * equip_item = (EquipmentListboxItem*)equipment_combobox_[i]->getListboxItemFromIndex(item);
            std::string equip_code = EQUIPMENT_SLOTS[i] + toString(equip_item->getID());
            if((accepted_comboboxes_[i] == equip_code) && equip_item->available_)
            {
                equipment_combobox_[i]->setText(equip_item->getText());                    
            }
        }
    }

    // set preview images on first selection
    showPreviewImage(preview_images_[1], accepted_comboboxes_[0] +
                                         accepted_comboboxes_[1] );
    
    showPreviewImage(preview_images_[0], accepted_comboboxes_[2] +
                                         accepted_comboboxes_[3] );

}

//------------------------------------------------------------------------------
void GUITankEquipment::registerCallbacks()
{
    equipment_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUITankEquipment::onKeySelect, this));

    cancel_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUITankEquipment::clickedCancelBtn, this));
    ok_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUITankEquipment::clickedOkBtn, this));

    for(unsigned i=0; i < NUM_EQUIPMENT_SLOTS; i++)
    {
        equipment_combobox_[i]->subscribeEvent(CEGUI::Combobox::EventListSelectionChanged, CEGUI::Event::Subscriber(&GUITankEquipment::comboboxListSelectionChanged, this));
        equipment_combobox_[i]->subscribeEvent(CEGUI::Combobox::EventListSelectionAccepted, CEGUI::Event::Subscriber(&GUITankEquipment::comboboxListSelectionAccepted, this));
    }

    s_gui.subscribeActivationEvents(equipment_window_);
}

//------------------------------------------------------------------------------
void GUITankEquipment::showPreviewImage(CEGUI::Window * preview_image, const std::string & name)
{
    if(name.empty())
    {
        preview_image->setProperty("Image", CEGUI::PropertyHelper::imageToString(NULL));
        return;
    }

    // show minimap preview if possible
    try
    {
        std::string full_image_name = BASE_TEX_PATH + "gui/" + name + ".png";

        CEGUI::Imageset * imageset = NULL;

         // if image has been loaded, take it from imageset otherwise generate imageset
        if(CEGUI::ImagesetManager::getSingleton().isImagesetPresent(full_image_name) == false)
        {
            CEGUI::Texture * preview_tex = CEGUI::System::getSingleton().getRenderer()->createTexture(full_image_name, "");
            imageset = CEGUI::ImagesetManager::getSingleton().createImageset(full_image_name, preview_tex);
            imageset->defineImage( full_image_name, 
                                   CEGUI::Point(0.0f, 0.0f), 
                                   CEGUI::Size(preview_tex->getWidth(), preview_tex->getHeight()), 
                                   CEGUI::Point(0.0f,0.0f));
        }
        else
        {
            imageset = CEGUI::ImagesetManager::getSingleton().getImageset(full_image_name);
        }

        CEGUI::String img = CEGUI::PropertyHelper::imageToString(&imageset->getImage(full_image_name));
        preview_image->setProperty("Image", img);
    }
    catch(CEGUI::Exception e)
    {
        s_log << Log::warning << "Unable to load preview in GUITankEquipment. Exception:" 
              << e.getMessage().c_str() << "\n";

        preview_image->setProperty("Image", CEGUI::PropertyHelper::imageToString(NULL));
    }
}

//------------------------------------------------------------------------------
void GUITankEquipment::setDescriptionText(EquipmentListboxItem * item)
{
    if(!item)
    {
        desc_window_->setText("");
        return;
    }

    if(!item->available_)
    {
        desc_window_->setText("NOT AVAILABLE IN THE BETA VERSION.\n\n" + item->description_);
    }
    else
    {
        desc_window_->setText(item->description_);
    }
}

//------------------------------------------------------------------------------
bool GUITankEquipment::clickedCancelBtn(const CEGUI::EventArgs& e)
{
    toggleShow();
    return true;
}

//------------------------------------------------------------------------------
bool GUITankEquipment::clickedOkBtn(const CEGUI::EventArgs& e)
{
    CEGUI::ListboxItem * current_combo_editbox_item = NULL;
    bool everything_selected = true;
    std::vector<uint8_t> equipment;

    // primary weapon
    current_combo_editbox_item = equipment_combobox_[0]->findItemWithText(equipment_combobox_[0]->getText(), NULL);
    if(current_combo_editbox_item)
    {
        EquipmentListboxItem * item = (EquipmentListboxItem*)current_combo_editbox_item;
        equipment.push_back(item->getID());
    }
    else
    {
        everything_selected = false;
    }


    // secondary weapon
    current_combo_editbox_item = equipment_combobox_[1]->findItemWithText(equipment_combobox_[1]->getText(), NULL);
    if(current_combo_editbox_item)
    {
        EquipmentListboxItem * item = (EquipmentListboxItem*)current_combo_editbox_item;
        equipment.push_back(item->getID());
    }
    else
    {
        everything_selected = false;
    }


    // skill 1
    current_combo_editbox_item = equipment_combobox_[2]->findItemWithText(equipment_combobox_[2]->getText(), NULL);
    if(current_combo_editbox_item)
    {
        EquipmentListboxItem * item = (EquipmentListboxItem*)current_combo_editbox_item;
        equipment.push_back(item->getID());
    }
    else
    {
        everything_selected = false;
    }

    // skill 2
    current_combo_editbox_item = equipment_combobox_[3]->findItemWithText(equipment_combobox_[3]->getText(), NULL);
    if(current_combo_editbox_item)
    {
        EquipmentListboxItem * item = (EquipmentListboxItem*)current_combo_editbox_item;
        equipment.push_back(item->getID());
    }
    else
    {
        everything_selected = false;
    }

    // selection successful
    if(everything_selected)
    {
        GameLogicClientCommon * glcc = (GameLogicClientCommon*)puppet_master_->getGameLogic();
        glcc->sendEquipmentChangeRequest(equipment);
        toggleShow();

        // store selection
        s_params.set<std::vector<std::string> >("player.equipment", accepted_comboboxes_);
        s_params.saveParameters(getUserConfigFile(), CONFIG_SUPERSECTION);
    }

    return true;
}

//------------------------------------------------------------------------------
bool GUITankEquipment::onKeySelect(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
        case CEGUI::Key::Escape:
            return clickedCancelBtn(e);
            break;
        case CEGUI::Key::Return:
        case CEGUI::Key::Space:
            return clickedOkBtn(e);
            break;
        default:
            return false;
    }    
}

//------------------------------------------------------------------------------
bool GUITankEquipment::comboboxListSelectionChanged(const CEGUI::EventArgs& e)
{
	const CEGUI::WindowEventArgs &window_event_args = static_cast<const CEGUI::WindowEventArgs&>(e);

    CEGUI::Combobox * combobox = static_cast<CEGUI::Combobox *>(window_event_args.window);

    EquipmentListboxItem * item = NULL;

    item = dynamic_cast<EquipmentListboxItem *>(combobox->getSelectedItem());

    if(item)
    {
        // check which combobox is selected and show the preview image
        for(unsigned i=0; i < NUM_EQUIPMENT_SLOTS; i++)
        {
            if(combobox == equipment_combobox_[i])
            {
                //// XXX hack
                if(i == 0) // primary weapon
                {                    
                    showPreviewImage(preview_images_[1], EQUIPMENT_SLOTS[i] + toString(item->getID()) +
                                                         accepted_comboboxes_[1] );
                }

                //// XXX hack
                if(i == 1) // secondary weapon
                {
                    showPreviewImage(preview_images_[1], accepted_comboboxes_[0] +
                                                         EQUIPMENT_SLOTS[i] + toString(item->getID()));

                }

                //// XXX hack
                if(i == 2) // first skill
                {
                    
                    showPreviewImage(preview_images_[0], EQUIPMENT_SLOTS[i] + toString(item->getID()) +
                                                         accepted_comboboxes_[3] );

                }

                //// XXX hack
                if(i == 3) // second skill
                {
                    showPreviewImage(preview_images_[0], accepted_comboboxes_[2]  +
                                                         EQUIPMENT_SLOTS[i] + toString(item->getID()) );

                }


            }
        }

        setDescriptionText(item);
    }
    else
    {
        // if nothing is currently selected
        // check which combobox is selected and show the preview image of the
        // currently accepted item
        for(unsigned i=0; i < NUM_EQUIPMENT_SLOTS; i++)
        {
            if(combobox == equipment_combobox_[i])
            {                 

                    //// XXX hack
                    if(i == 0 || i == 1) // primary weapon
                    {                    
                        showPreviewImage(preview_images_[1], accepted_comboboxes_[0] +
                                                             accepted_comboboxes_[1] );
                    }

                    //// XXX hack
                    if(i == 2 || i == 3) // first skill
                    {
                        
                        showPreviewImage(preview_images_[0], accepted_comboboxes_[2]  +
                                                             accepted_comboboxes_[3] );

                    }

            }
        }

        setDescriptionText(NULL);
    }

    return true;
}

//------------------------------------------------------------------------------
bool GUITankEquipment::comboboxListSelectionAccepted(const CEGUI::EventArgs& e)
{
	const CEGUI::WindowEventArgs &window_event_args = static_cast<const CEGUI::WindowEventArgs&>(e);

    CEGUI::Combobox * combobox = static_cast<CEGUI::Combobox *>(window_event_args.window);

    EquipmentListboxItem * item = NULL;

    item = dynamic_cast<EquipmentListboxItem *>(combobox->getSelectedItem());

        // check which combobox is selected and show the preview image
        for(unsigned i=0; i < NUM_EQUIPMENT_SLOTS; i++)
        {
            if(combobox == equipment_combobox_[i])
            {
                // set accepted string
                if(item)
                {
                    accepted_comboboxes_[i] = EQUIPMENT_SLOTS[i] + toString(item->getID());
                }

                // if unavailable item selected, set to first item
                if(item && !item->available_) 
                {
                    EquipmentListboxItem * new_selection = (EquipmentListboxItem*)combobox->getListboxItemFromIndex(0);
                    combobox->setItemSelectState(new_selection, true);
                    combobox->setText(new_selection->getText());

                    accepted_comboboxes_[i] = EQUIPMENT_SLOTS[i] + toString(new_selection->getID());
                }


            }
        }

    return true;
}
