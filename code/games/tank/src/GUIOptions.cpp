
#include "GUIOptions.h"

#include "MainMenu.h"
#include "Utils.h"
#include "SceneManager.h"

#include "SoundManager.h"
#include "Gui.h"
#include "SdlKeyNames.h"
#include "InputHandler.h"
#include "UserPreferences.h"
#include "Player.h" ///< needed for player name regexp
#include "RegEx.h"

const std::string KEY_INFO_USED = " key is already used for: ";


//------------------------------------------------------------------------------
GUIOptions::GUIOptions(MainMenu * main_menu) :
    res_combobox_(NULL),
    main_menu_(main_menu),
    capturing_input_(NULL)
{
    enableFloatingPointExceptions(false);

    loadWidgets();

    fillComboboxes();

    // set init values for keybinding multicolumn list
    key_list_->setSelectionMode(CEGUI::MultiColumnList::RowSingle);
    key_list_->setUserSortControlEnabled(false);
    key_list_->setUserColumnSizingEnabled(false);
    key_list_->setUserColumnDraggingEnabled(false);
    key_list_->setWantsMultiClickEvents(true);
    key_list_->setSortDirection(CEGUI::ListHeaderSegment::None);
    key_list_->addColumn("Action", 0, CEGUI::UDim(0.65f, 0));
    key_list_->addColumn("Key", 1, CEGUI::UDim(0.28f,0));
    
    // customize lists header
    key_list_->getListHeader()->getSegmentFromColumn(0).setProperty("Font","main_menu_font");
    key_list_->getListHeader()->getSegmentFromColumn(1).setProperty("Font","main_menu_font");

    showCapturingWindow(false);

    // make first tab visible on default
    options_window_->setVisible(false);
    tab1_window_->setVisible(true);
    tab2_window_->setVisible(false);
    tab3_window_->setVisible(false);
    tab4_window_->setVisible(false);
    tab5_window_->setVisible(false);

    registerCallbacks();

    // populate options menu with inital values
    loadOptions();

    // disable shader and shadow boxes if no shaders are possible
    if (!s_scene_manager.areShadersEnabled())
    {
        shader_quality_combobox_->disable();
        shadows_enabled_checkbox_->disable();
    }
    
    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
GUIOptions::~GUIOptions()
{
}

//------------------------------------------------------------------------------
void GUIOptions::show()
{
    options_window_->setVisible(true);
    options_window_->activate();
    loadOptions();
}

//------------------------------------------------------------------------------
void GUIOptions::hide()
{
    options_window_->setVisible(false);
}

//------------------------------------------------------------------------------
void GUIOptions::registerCallbacks()
{
    tab1_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIOptions::clickedTab1Btn, this));
    tab2_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIOptions::clickedTab2Btn, this));
    tab3_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIOptions::clickedTab3Btn, this));
    tab4_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIOptions::clickedTab4Btn, this));
    tab5_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIOptions::clickedTab5Btn, this));

    save_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIOptions::clickedSaveBtn, this));
    cancel_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIOptions::clickedCancelBtn, this));

    options_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUIOptions::onKeyDownOptionsWindow, this));

    player_name_editbox_->subscribeEvent(CEGUI::Editbox::EventTextChanged, CEGUI::Event::Subscriber(&GUIOptions::onPlayerNameChangeEditbox, this));

    // key binding & capturing
    key_list_->subscribeEvent(CEGUI::MultiColumnList::EventMouseDoubleClick, CEGUI::Event::Subscriber(&GUIOptions::dblClickedKeyList, this));
    capturing_window_->subscribeEvent(CEGUI::Window::EventMouseClick, CEGUI::Event::Subscriber(&GUIOptions::onMouseEventCapturing, this));
    capturing_window_->subscribeEvent(CEGUI::Window::EventMouseWheel, CEGUI::Event::Subscriber(&GUIOptions::onMouseEventCapturing, this));
    capturing_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUIOptions::onKeyDownCapturing, this));

    sound_volume_slider_->subscribeEvent(CEGUI::Slider::EventValueChanged,
                                         CEGUI::Event::Subscriber(&GUIOptions::volumeSliderChanged, this));

    music_checkbox_->subscribeEvent(CEGUI::Checkbox::EventCheckStateChanged,
                                         CEGUI::Event::Subscriber(&GUIOptions::musicCheckboxChanged, this));
}

//------------------------------------------------------------------------------
void GUIOptions::loadWidgets()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("MainMenu_root/");
    
    // Use parent window name as prefix to avoid name clashes
    wm.loadWindowLayout("options.layout", parent->getName());
    options_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "options/window");
    tab1_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "options/tab1_content");
    tab2_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "options/tab2_content");
    tab3_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "options/tab3_content");
    tab4_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "options/tab4_content");
    tab5_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "options/tab5_content");
    tab1_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "options/tab1_btn");
    tab2_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "options/tab2_btn");
    tab3_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "options/tab3_btn");
    tab4_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "options/tab4_btn");
    tab5_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "options/tab5_btn");

    save_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "options/save_btn");
    cancel_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "options/cancel_btn");

    invert_mouse_checkbox_ = (CEGUI::Checkbox*) wm.getWindow(parent->getName() + "options/tab1_content/invert_mouse_checkbox");
    res_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab2_content/res_combobox");
    mouse_sens_slider_ = (CEGUI::Slider*) wm.getWindow(parent->getName() + "options/tab1_content/mouse_sens_slider"); 
    player_name_editbox_ = (CEGUI::Editbox*) wm.getWindow(parent->getName() + "options/tab1_content/player_name_editbox");
    fullscreen_checkbox_ = (CEGUI::Checkbox*) wm.getWindow(parent->getName() + "options/tab2_content/fullscreen_checkbox");
    sound_volume_slider_ = (CEGUI::Slider*) wm.getWindow(parent->getName() + "options/tab3_content/volume_slider");
    sound_device_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab3_content/sound_device_combobox");
    music_checkbox_ = (CEGUI::Checkbox*) wm.getWindow(parent->getName() + "options/tab3_content/music_checkbox");
    shadows_enabled_checkbox_ = (CEGUI::Checkbox*) wm.getWindow(parent->getName() + "options/tab4_content/shadows_enabled_checkbox");
    shadows_quality_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab4_content/shadows_quality_combobox");
    fsaa_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab4_content/fsaa_combobox");
    tex_quality_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab4_content/tex_quality_combobox");
    shader_quality_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab4_content/shader_quality_combobox");
    anisotropic_filtering_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab4_content/anisotropic_filtering_combobox");
    view_distance_slider_ = (CEGUI::Slider*) wm.getWindow(parent->getName() + "options/tab4_content/view_distance_slider");

    view_distance_slider_->setMaxValue(s_params.get<float>("client.graphics.max_lod_scale") -
                                       s_params.get<float>("client.graphics.min_lod_scale"));
    
    grass_density_slider_ = (CEGUI::Slider*) wm.getWindow(parent->getName() + "options/tab4_content/grass_density_slider");
    terrain_view_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab4_content/terrain_view_combobox");
    water_quality_combobox_ = (CEGUI::Combobox*) wm.getWindow(parent->getName() + "options/tab4_content/water_quality_combobox");

    key_list_ = (CEGUI::MultiColumnList*) wm.getWindow(parent->getName() + "options/tab5_content/key_list");
    key_info_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "options/capturing_window/key_info");
    capturing_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "options/capturing_window");


    // restrict editbox input
    player_name_editbox_->setValidationString("([" +
                                             PLAYER_NAME_SPECIAL_CHARS + 
                                             PLAYER_NAME_ALLOWED_CHARS + 
                                             "\\s]{0,20})");

    // add options to widget tree
    parent->addChildWindow(options_window_);
}

//------------------------------------------------------------------------------
void GUIOptions::fillComboboxes()
{
    // XXX Adding listbox items in layout is not supported by CEGUI yet.
    // fill resolution combobox
    CEGUI::ListboxTextItem * itm;
    itm = new ResolutionListboxItem("800x600", 0, 800, 600);
    res_combobox_->addItem(itm);
    itm = new ResolutionListboxItem("1024x768", 1, 1024, 768);
    res_combobox_->addItem(itm);
    itm = new ResolutionListboxItem("1152x864", 2, 1152, 864);
    res_combobox_->addItem(itm);
    itm = new ResolutionListboxItem("1280x800", 3, 1280, 800);
    res_combobox_->addItem(itm);
    itm = new ResolutionListboxItem("1280x960", 4, 1280, 960);
    res_combobox_->addItem(itm);
    itm = new ResolutionListboxItem("1280x1024", 5, 1280, 1024);
    res_combobox_->addItem(itm);
    itm = new ResolutionListboxItem("1600x1200", 6, 1600, 1200);
    res_combobox_->addItem(itm);
    itm = new ResolutionListboxItem("1680x1050", 7, 1680, 1050);
    res_combobox_->addItem(itm);
    itm = new ResolutionListboxItem("1920x1200", 8, 1920, 1200);
    res_combobox_->addItem(itm);

    // fill sound device combobox
    SoundManager::SoundDeviceMap::const_iterator it;
    SoundManager::SoundDeviceMap devices = s_soundmanager.getAvailableSoundDevices();
    for(it = devices.begin(); it != devices.end(); it++)
    {
        itm = new SoundDeviceListboxItem(it->second, it->first, it->first);
        sound_device_combobox_->addItem(itm);
    }


    // fill shadow quality combobox
    itm = new ShadowMapListboxItem("Low", 0, 512);
    shadows_quality_combobox_->addItem(itm);
    itm = new ShadowMapListboxItem("Medium", 1, 1024);
    shadows_quality_combobox_->addItem(itm);
    itm = new ShadowMapListboxItem("High", 2, 2048);
    shadows_quality_combobox_->addItem(itm);

    // fill FSAA combobox
    itm = new FSAAListboxItem("Off", 0, 0);
    fsaa_combobox_->addItem(itm);
    itm = new FSAAListboxItem("2x", 1, 2);
    fsaa_combobox_->addItem(itm);
    itm = new FSAAListboxItem("4x", 2, 4);
    fsaa_combobox_->addItem(itm);
    itm = new FSAAListboxItem("6x", 3, 6);
    fsaa_combobox_->addItem(itm);
    itm = new FSAAListboxItem("8x", 4, 8);
    fsaa_combobox_->addItem(itm);

    // fill anisotropic filtering combobox, take only possible values
    // depending on supported max value
    itm = new AFListboxItem("Off", 0, 0);
    anisotropic_filtering_combobox_->addItem(itm);
    unsigned max_anisotropic_supported = (unsigned)s_scene_manager.getMaxSupportedAnisotropy();
    if(max_anisotropic_supported >= 2)
    {
        itm = new AFListboxItem("2x", 1, 2);
        anisotropic_filtering_combobox_->addItem(itm);
    }
    if(max_anisotropic_supported >= 4)
    {
        itm = new AFListboxItem("4x", 2, 4);
        anisotropic_filtering_combobox_->addItem(itm);
    }
    if(max_anisotropic_supported >= 8)
    {
        itm = new AFListboxItem("8x", 3, 8);
        anisotropic_filtering_combobox_->addItem(itm);
    }
    if(max_anisotropic_supported >= 16)
    {
        itm = new AFListboxItem("16x", 4, 16);
        anisotropic_filtering_combobox_->addItem(itm);
    }


    // fill texture quality combobox
    itm = new TexQualityListboxItem("Low", 0, 2);
    tex_quality_combobox_->addItem(itm);
    itm = new TexQualityListboxItem("Medium", 1, 1);
    tex_quality_combobox_->addItem(itm);
    itm = new TexQualityListboxItem("High", 2, 0);
    tex_quality_combobox_->addItem(itm);

    // fill shader quality combobox
    itm = new ShaderQualityListboxItem("Low", 0, 0);
    shader_quality_combobox_->addItem(itm);
    itm = new ShaderQualityListboxItem("Medium", 1, 1);
    shader_quality_combobox_->addItem(itm);
    itm = new ShaderQualityListboxItem("High", 2, 2);
    shader_quality_combobox_->addItem(itm);
    
    // Terrain View combobox
    itm = new TerrainViewListboxItem("Low",    0, 1, 4, 3);
    terrain_view_combobox_->addItem(itm);
    itm = new TerrainViewListboxItem("Medium", 1, 0, 5, 3);
    terrain_view_combobox_->addItem(itm);
    itm = new TerrainViewListboxItem("High",   2, 0, 4, 4);
    terrain_view_combobox_->addItem(itm);

    // Water Quality combobox
    itm = new WaterQualityListboxItem("Low", 0, 0);
    water_quality_combobox_->addItem(itm);
    itm = new WaterQualityListboxItem("Medium", 1, 1);
    water_quality_combobox_->addItem(itm);
    itm = new WaterQualityListboxItem("High", 2, 2);
    water_quality_combobox_->addItem(itm);

}

//------------------------------------------------------------------------------
bool GUIOptions::clickedTab1Btn(const CEGUI::EventArgs& e)
{   
    tab1_window_->setVisible(true);
    tab2_window_->setVisible(false);
    tab3_window_->setVisible(false);
    tab4_window_->setVisible(false);
    tab5_window_->setVisible(false);
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::clickedTab2Btn(const CEGUI::EventArgs& e)
{
    tab1_window_->setVisible(false);
    tab2_window_->setVisible(true);
    tab3_window_->setVisible(false);
    tab4_window_->setVisible(false);
    tab5_window_->setVisible(false);
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::clickedTab3Btn(const CEGUI::EventArgs& e)
{
    tab1_window_->setVisible(false);
    tab2_window_->setVisible(false);
    tab3_window_->setVisible(true);
    tab4_window_->setVisible(false);
    tab5_window_->setVisible(false);
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::clickedTab4Btn(const CEGUI::EventArgs& e)
{
    tab1_window_->setVisible(false);
    tab2_window_->setVisible(false);
    tab3_window_->setVisible(false);
    tab4_window_->setVisible(true);
    tab5_window_->setVisible(false);
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::clickedTab5Btn(const CEGUI::EventArgs& e)
{
    tab1_window_->setVisible(false);
    tab2_window_->setVisible(false);
    tab3_window_->setVisible(false);
    tab4_window_->setVisible(false);
    tab5_window_->setVisible(true);
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::clickedSaveBtn(const CEGUI::EventArgs& e)
{
    saveOptions();

    hide();
    main_menu_->showMainMenu();
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::clickedCancelBtn(const CEGUI::EventArgs& e)
{
    loadOptions();

    hide();
    main_menu_->showMainMenu();
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::dblClickedKeyList(const CEGUI::EventArgs& e)
{
    capturing_input_ = (KeyMapElement*)key_list_->getFirstSelectedItem();

    if(capturing_input_) // if element selected
    {
        unsigned current_row = key_list_->getItemRowIndex(capturing_input_);

        // Bail if a header was double-clicked
        if (key_list_->getItemAtGridReference(CEGUI::MCLGridRef(current_row,1))->getText() != "")
        {
            key_info_->setText("");
            showCapturingWindow(true);
        }
    }
    
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::volumeSliderChanged(const CEGUI::EventArgs& e)
{
    s_soundmanager.setListenerGain(clamp(sound_volume_slider_->getCurrentValue(), 0.0f, 1.0f));
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::musicCheckboxChanged(const CEGUI::EventArgs& e)
{
    if(music_checkbox_->isSelected())
    {
        main_menu_->startMusic(false);
    }
    else
    {
        main_menu_->stopMusic(false);
    }

    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::onPlayerNameChangeEditbox(const CEGUI::EventArgs& e)
{
    std::string pn = player_name_editbox_->getText().c_str();
    RegEx reg_ex(PLAYER_NAME_REGEX);
    if(reg_ex.match(pn))
    {
        player_name_editbox_->setProperty("NormalTextColour","FFFFFFFF");
    }
    else
    {
        player_name_editbox_->setProperty("NormalTextColour","FFDD0000");
    }
    
    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::onKeyDownOptionsWindow(const CEGUI::EventArgs& e)
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
bool GUIOptions::onKeyDownCapturing(const CEGUI::EventArgs& e)
{
    CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    // key capturing
    if(capturing_input_)
    {
        if(ek->scancode == CEGUI::Key::Escape) // stop capturing on ESC
        {
            capturing_input_ = NULL;
            showCapturingWindow(false);
            return true;
        }
        
        const input_handler::Key & key =
            input_handler::getKeyWithCode(s_gui.CEGUIKeyToSDLKey(ek->scancode));
        
        if (key.code_ != input_handler::INVALID_KEY_CODE)
        {
            changeKeybinding(key.name_);
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
bool GUIOptions::onMouseEventCapturing(const CEGUI::EventArgs& e)
{
    CEGUI::MouseEventArgs* em = (CEGUI::MouseEventArgs*)&e;

    if(capturing_input_)
    {
        std::string mouse_event = "";

        if(em->wheelChange > 0.0f) mouse_event = "WheelUp";
        if(em->wheelChange < 0.0f) mouse_event = "WheelDown";

        if(em->button == CEGUI::LeftButton) mouse_event = "LeftButton";
        if(em->button == CEGUI::RightButton) mouse_event = "RightButton";
        if(em->button == CEGUI::MiddleButton) mouse_event = "MiddleButton";

        if(!mouse_event.empty()) changeKeybinding(mouse_event);
    }

    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::loadOptions()
{

    // player name
    player_name_editbox_->setText(s_params.get<std::string>("client.app.player_name"));

    if(!player_name_editbox_->isTextValid()) player_name_editbox_->setText("Player");

    // invert mouse
    invert_mouse_checkbox_->setSelected(s_params.get<bool>("camera.invert_mouse"));

    // mouse sensitivity       
    mouse_sens_slider_->setCurrentValue(clamp(s_params.get<float>("client.input.mouse_sensitivity"),0.0f,1.0f));

    // resolution settings
    unsigned width = s_params.get<unsigned>("client.app.initial_window_width");
    unsigned height = s_params.get<unsigned>("client.app.initial_window_height");

    for(size_t item=0; item < res_combobox_->getItemCount(); ++item)
    {
        ResolutionListboxItem * res_item = (ResolutionListboxItem*)res_combobox_->getListboxItemFromIndex(item);
        if(res_item->width_ == width && res_item->height_ == height)
        {
            res_combobox_->setItemSelectState(item, true);
            res_combobox_->setText(res_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // fullscreen
    fullscreen_checkbox_->setSelected(s_params.get<bool>("client.app.fullscreen"));

    // sound device
    unsigned sound_device = s_params.get<unsigned>("client.sound.device_index");
    for(size_t item=0; item < sound_device_combobox_->getItemCount(); ++item)
    {
        SoundDeviceListboxItem * sound_device_item = (SoundDeviceListboxItem*)sound_device_combobox_->getListboxItemFromIndex(item);
        if(sound_device_item->device_index_ == sound_device)
        {
            sound_device_combobox_->setItemSelectState(item, true);
            sound_device_combobox_->setText(sound_device_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }


    // sound volume
    float volume = clamp(s_params.get<float>("client.sound.volume"),0.0f,1.0f);
    sound_volume_slider_->setCurrentValue(volume);
    s_soundmanager.setListenerGain(volume); // set initial listener gain, for music in main menu

    // music
    music_checkbox_->setSelected(s_params.get<bool>("client.music.enabled"));

    
    // shadows enabled
    bool shadows_enabled;
    if (s_scene_manager.areShadersEnabled())
    {
        shadows_enabled = s_params.get<bool>("client.shadows.enabled");
    } else
    {
        shadows_enabled = false;
    }
    shadows_enabled_checkbox_->setSelected(shadows_enabled);


    unsigned shadow_map_size = s_params.get<unsigned>("client.shadows.map_size");

    for(size_t item=0; item < shadows_quality_combobox_->getItemCount(); ++item)
    {
        ShadowMapListboxItem * shadow_quality_item = (ShadowMapListboxItem*)shadows_quality_combobox_->getListboxItemFromIndex(item);
        if(shadow_quality_item->shadow_map_size_ == shadow_map_size)
        {
            shadows_quality_combobox_->setItemSelectState(item, true);
            shadows_quality_combobox_->setText(shadows_quality_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // FSAA
    unsigned fsaa_samples = s_params.get<unsigned>("client.graphics.fsaa_samples");

    for(size_t item=0; item < fsaa_combobox_->getItemCount(); ++item)
    {
        FSAAListboxItem * fsaa_item = (FSAAListboxItem*)fsaa_combobox_->getListboxItemFromIndex(item);
        if(fsaa_item->fsaa_samples_ == fsaa_samples)
        {
            fsaa_combobox_->setItemSelectState(item, true);
            fsaa_combobox_->setText(fsaa_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // Anisotropic Filtering
    unsigned af_samples = s_params.get<unsigned>("client.graphics.anisotropic_filtering");

    for(size_t item=0; item < anisotropic_filtering_combobox_->getItemCount(); ++item)
    {
        AFListboxItem * af_samples_item = (AFListboxItem*)anisotropic_filtering_combobox_->getListboxItemFromIndex(item);
        if(af_samples_item->af_samples_ == af_samples)
        {
            anisotropic_filtering_combobox_->setItemSelectState(item, true);
            anisotropic_filtering_combobox_->setText(anisotropic_filtering_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // Texture Quality
    unsigned tex_quality = s_params.get<unsigned>("client.graphics.texture_quality");

    for(size_t item=0; item < tex_quality_combobox_->getItemCount(); ++item)
    {
        TexQualityListboxItem * tex_quality_item = (TexQualityListboxItem*)tex_quality_combobox_->getListboxItemFromIndex(item);
        if(tex_quality_item->tex_quality_ == tex_quality)
        {
            tex_quality_combobox_->setItemSelectState(item, true);
            tex_quality_combobox_->setText(tex_quality_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // Shader Quality
    unsigned shader_quality;
    if (s_scene_manager.areShadersEnabled())
    {
        shader_quality = s_params.get<unsigned>("client.graphics.shader_quality");
    } else
    {
        shader_quality = 0;
    }

    for(size_t item=0; item < shader_quality_combobox_->getItemCount(); ++item)
    {
        ShaderQualityListboxItem * shader_quality_item = (ShaderQualityListboxItem*)shader_quality_combobox_->getListboxItemFromIndex(item);
        if(shader_quality_item->shader_quality_ == shader_quality)
        {
            shader_quality_combobox_->setItemSelectState(item, true);
            shader_quality_combobox_->setText(shader_quality_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // Terrain View Distance
    unsigned start_grid = s_params.get<unsigned>("terrain.start_grid");
    unsigned num_grids  = s_params.get<unsigned>("terrain.num_grids");
    unsigned grid_size  = s_params.get<unsigned>("terrain.grid_size");

    for(size_t item=0; item < terrain_view_combobox_->getItemCount(); ++item)
    {
        TerrainViewListboxItem * terrain_view_item = (TerrainViewListboxItem*)terrain_view_combobox_->getListboxItemFromIndex(item);
        if( terrain_view_item->start_grid_ == start_grid &&
            terrain_view_item->num_grids_  == num_grids &&
            terrain_view_item->grid_size_  == grid_size)            
        {
            terrain_view_combobox_->setItemSelectState(item, true);
            terrain_view_combobox_->setText(terrain_view_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }

    // Water Quality
    unsigned water_quality;
    water_quality = s_params.get<unsigned>("client.graphics.water_quality");
        

    for(size_t item=0; item < water_quality_combobox_->getItemCount(); ++item)
    {
        WaterQualityListboxItem * water_quality_item = (WaterQualityListboxItem*)water_quality_combobox_->getListboxItemFromIndex(item);
        if(water_quality_item->water_quality_ == water_quality)
        {
            water_quality_combobox_->setItemSelectState(item, true);
            water_quality_combobox_->setText(water_quality_combobox_->getListboxItemFromIndex(item)->getText());
        }
    }


    // View Distance
    float distance = s_params.get<float>("client.graphics.lod_scale");
    view_distance_slider_->setCurrentValue(clamp(distance-s_params.get<float>("client.graphics.min_lod_scale"),
                                                 0.0f,
                                                 s_params.get<float>("client.graphics.max_lod_scale")));

    // Grass Density
    float density = clamp(s_params.get<float>("instances.user_density"), 0.0f, 1.0f);
    grass_density_slider_->setCurrentValue(density);

    // fill the key map with elements
    key_list_->resetList();     // clear key_list_ first
    
    keymap_params_.loadParameters(getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);

    const StringParamMap & param_map =
        keymap_params_.getParameterMap<std::vector<std::string> >();
    
    for (StringParamMap::const_iterator it = param_map.begin();
         it != param_map.end();
         ++it)
    {
        if (it->first.length() <= input_handler::ACTION_PREFIX_LENGTH)
        {
            s_log << Log::warning
                  << "Malformed entry in keymap: "
                  << it->first
                  << "\n";
            continue;
        }
        
        if (it->second.empty()) addKeybindHeader(it->first.substr(input_handler::ACTION_PREFIX_LENGTH));
        else                    addKeybindRow   (it->first.substr(0, input_handler::ACTION_PREFIX_LENGTH),
                                                 it->first.substr(input_handler::ACTION_PREFIX_LENGTH),
                                                 it->second[0]);
    }

    return true;
}

//------------------------------------------------------------------------------
bool GUIOptions::saveOptions()
{
    CEGUI::ListboxItem * current_combo_editbox_item = NULL;

    // player name
    std::string pn = player_name_editbox_->getText().c_str();
    RegEx reg_ex(PLAYER_NAME_REGEX);
    if(reg_ex.match(pn))
    {
        s_params.set<std::string>("client.app.player_name", pn);

        // if client is up and running, change name also in-game
        if(main_menu_->isClientRunning())
        {
            s_console.executeCommand(("name " + pn).c_str());
        }
    }

    // invert mouse
    s_params.set("camera.invert_mouse",invert_mouse_checkbox_->isSelected());

    // mouse sensitivity      
    s_params.set("client.input.mouse_sensitivity", mouse_sens_slider_->getCurrentValue());

    // screen resolution    
    current_combo_editbox_item = res_combobox_->findItemWithText(res_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        ResolutionListboxItem * res_item = (ResolutionListboxItem*)current_combo_editbox_item;

        s_params.set("client.app.initial_window_width", res_item->width_);
        s_params.set("client.app.initial_window_height", res_item->height_);
    }

    // fullscreen
    s_params.set("client.app.fullscreen", fullscreen_checkbox_->isSelected());

    // sound device
    current_combo_editbox_item = sound_device_combobox_->findItemWithText(sound_device_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        SoundDeviceListboxItem * sound_device_item = (SoundDeviceListboxItem*)current_combo_editbox_item;
        s_params.set("client.sound.device_index", sound_device_item->device_index_);
    }

    // sound volume
    s_params.set("client.sound.volume",(float)clamp(sound_volume_slider_->getCurrentValue(), 0.0f, 1.0f));
    s_soundmanager.setListenerGain(clamp(sound_volume_slider_->getCurrentValue(), 0.0f, 1.0f));

    // music
    s_params.set<bool>("client.music.enabled", music_checkbox_->isSelected());

    // shadows enabled
    s_params.set("client.shadows.enabled",shadows_enabled_checkbox_->isSelected());


    // shadow quality, aka map size
    current_combo_editbox_item = shadows_quality_combobox_->findItemWithText(shadows_quality_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        ShadowMapListboxItem * shadow_quality_item = (ShadowMapListboxItem*)current_combo_editbox_item;
        s_params.set("client.shadows.map_size", shadow_quality_item->shadow_map_size_);
    }

    // FSAA samples
    current_combo_editbox_item = fsaa_combobox_->findItemWithText(fsaa_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        FSAAListboxItem * fsaa_item = (FSAAListboxItem*)current_combo_editbox_item;
        s_params.set("client.graphics.fsaa_samples", fsaa_item->fsaa_samples_);
    }

    // Anisotropic filtering
    current_combo_editbox_item = anisotropic_filtering_combobox_->findItemWithText(anisotropic_filtering_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        AFListboxItem * af_item = (AFListboxItem*)current_combo_editbox_item;
        s_params.set("client.graphics.anisotropic_filtering", af_item->af_samples_);
    }

    // Texture Quality 
    current_combo_editbox_item = tex_quality_combobox_->findItemWithText(tex_quality_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        TexQualityListboxItem * tex_quality_item = (TexQualityListboxItem*)current_combo_editbox_item;
        s_params.set("client.graphics.texture_quality", tex_quality_item->tex_quality_);
    }

    // Shader Quality 
    current_combo_editbox_item = shader_quality_combobox_->findItemWithText(shader_quality_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        ShaderQualityListboxItem * shader_quality_item = (ShaderQualityListboxItem*)current_combo_editbox_item;
        s_params.set("client.graphics.shader_quality", shader_quality_item->shader_quality_);
    }

    // Terrain View Distance
    current_combo_editbox_item = terrain_view_combobox_->findItemWithText(terrain_view_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        TerrainViewListboxItem * terrain_view_item = (TerrainViewListboxItem*)current_combo_editbox_item;
        s_params.set("terrain.start_grid", terrain_view_item->start_grid_);
        s_params.set("terrain.num_grids",  terrain_view_item->num_grids_);
        s_params.set("terrain.grid_size",  terrain_view_item->grid_size_);
    }

    // Water Quality 
    current_combo_editbox_item = water_quality_combobox_->findItemWithText(water_quality_combobox_->getText(), NULL);
    if(current_combo_editbox_item)
    {
        WaterQualityListboxItem * water_quality_item = (WaterQualityListboxItem*)current_combo_editbox_item;
        s_params.set<unsigned>("client.graphics.water_quality", water_quality_item->water_quality_);
    }

    // View Distance    
    float distance = view_distance_slider_->getCurrentValue() + s_params.get<float>("client.graphics.min_lod_scale");
    s_params.set("client.graphics.lod_scale", distance);

    // Grass Density
    float density = clamp(grass_density_slider_->getCurrentValue(), 0.0f, 1.0f);
    s_params.set("instances.user_density", density);

    // XXXX write application specific load/save file logic?
    // actually write values to file
    s_params.saveParameters(getUserConfigFile(), CONFIG_SUPERSECTION);


    keymap_params_.saveParameters(getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);
    
    s_input_handler.loadKeymap  (getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);
    
    return true;
}

//------------------------------------------------------------------------------
void GUIOptions::addKeybindRow(const std::string & prefix,
                               const std::string & action_name,
                               const std::string & key_name)
{
	unsigned row = key_list_->addRow();
	key_list_->setItem(new KeyMapElement(action_name, prefix), 0, row);
	key_list_->setItem(new KeyMapElement(key_name,     ""), 1, row);
}

//------------------------------------------------------------------------------
void GUIOptions::addKeybindHeader(const std::string & name)
{
	unsigned row = key_list_->addRow();
	key_list_->setItem(new KeyMapElement("-------------------- " + name + " --------------------",  ""), 0, row);
	key_list_->setItem(new KeyMapElement("",    ""), 1, row);
}

//------------------------------------------------------------------------------
/**
 *  Assignes the specified key to the action contained in the
 *  capturing_input_ map element.
 */
void GUIOptions::changeKeybinding(const std::string & key_name)
{
    if(!capturing_input_) return;

    unsigned current_row = key_list_->getItemRowIndex(capturing_input_);

    // go through list and check if key is already used
    for(unsigned c=0; c < key_list_->getRowCount(); c++)
    {
        // allow re-assignment of same key
        if (c==current_row) continue;
        
        if(key_list_->getItemAtGridReference(CEGUI::MCLGridRef(c,1))->getText() ==
            CEGUI::String(key_name))
        {
            key_info_->setText(key_name + KEY_INFO_USED + key_list_->getItemAtGridReference(CEGUI::MCLGridRef(c,0))->getText());
            return;
        }
    }
    
    // if key is unused yet, assign it to gui list and update local params
    KeyMapElement * elem = dynamic_cast<KeyMapElement*>(key_list_->getItemAtGridReference(CEGUI::MCLGridRef(current_row,0)));
    assert(elem);
    std::vector<std::string> key_values;
    key_values.push_back(key_name);

    keymap_params_.set(elem->prefix_ + elem->getText().c_str(), key_values); // write to params

    key_list_->getItemAtGridReference(CEGUI::MCLGridRef(current_row,1))->setText(key_name);
    key_list_->handleUpdatedItemData();
    key_info_->setText("");

    showCapturingWindow(false);
    capturing_input_ = NULL;
}

//------------------------------------------------------------------------------
void GUIOptions::showCapturingWindow(bool v)
{
    v ? capturing_window_->activate() : capturing_window_->deactivate();
    capturing_window_->setVisible(v);
}
