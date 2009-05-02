
#ifndef TANK_GUIOPTIONS_INCLUDED
#define TANK_GUIOPTIONS_INCLUDED

#include <CEGUI/CEGUI.h>

#include "ParameterManager.h"

class MainMenu;
class KeyMapElement;

const std::string LOOK_N_FEEL = "WindowsLook";
const std::string LISTBOX_ITEM_FONT = "main_menu_font";
const std::string LISTBOX_ITEM_FONT_SMALL = "main_menu_small_font";
const CEGUI::colour LISTBOX_ITEM_SELECTION_COLOR = CEGUI::colour(1,1,1);
const CEGUI::colour LISTBOX_ITEM_TEXT_COLOR = CEGUI::colour(1,1,1);
const CEGUI::colour LISTBOX_ITEM_TEXT_DISABLED_COLOR = CEGUI::colour(0.8,0.1,0.1);



//------------------------------------------------------------------------------
class GUIOptions
{
 public:
    GUIOptions(MainMenu * main_menu);
    virtual ~GUIOptions();
    
    void show();
    void hide();

 protected:

    typedef std::map<std::string, std::vector<std::string> > StringParamMap;

    void registerCallbacks();
    void loadWidgets();
    void fillComboboxes();

    bool clickedTab1Btn(const CEGUI::EventArgs& e);
    bool clickedTab2Btn(const CEGUI::EventArgs& e);
    bool clickedTab3Btn(const CEGUI::EventArgs& e);
    bool clickedTab4Btn(const CEGUI::EventArgs& e);
    bool clickedTab5Btn(const CEGUI::EventArgs& e);
    bool clickedSaveBtn(const CEGUI::EventArgs& e);
    bool clickedCancelBtn(const CEGUI::EventArgs& e);
    bool dblClickedKeyList(const CEGUI::EventArgs& e);
    bool volumeSliderChanged(const CEGUI::EventArgs& e);
    bool musicCheckboxChanged(const CEGUI::EventArgs& e);

    bool onPlayerNameChangeEditbox(const CEGUI::EventArgs& e);
    bool onKeyDownOptionsWindow(const CEGUI::EventArgs& e);
    bool onKeyDownCapturing(const CEGUI::EventArgs& e);
    bool onMouseEventCapturing(const CEGUI::EventArgs& e);


    bool loadOptions();
    bool saveOptions();

    void addKeybindRow(const std::string & prefix,
                       const std::string & action_name,
                       const std::string & key_name);
    void addKeybindHeader(const std::string & name);
    void changeKeybinding(const std::string & key_name);

    void showCapturingWindow(bool v);

    CEGUI::Window * options_window_;
    CEGUI::Window * tab1_window_;
    CEGUI::Window * tab2_window_;
    CEGUI::Window * tab3_window_;
    CEGUI::Window * tab4_window_;
    CEGUI::Window * tab5_window_;
    CEGUI::ButtonBase * tab1_btn_;
    CEGUI::ButtonBase * tab2_btn_;
    CEGUI::ButtonBase * tab3_btn_;
    CEGUI::ButtonBase * tab4_btn_;
    CEGUI::ButtonBase * tab5_btn_;
    CEGUI::ButtonBase * cancel_btn_;
    CEGUI::ButtonBase * save_btn_;

    CEGUI::MultiColumnList * key_list_;
    CEGUI::Window * key_info_;
    CEGUI::Window * capturing_window_;

    CEGUI::Editbox * player_name_editbox_;
    CEGUI::Checkbox * invert_mouse_checkbox_;
    CEGUI::Slider * mouse_sens_slider_;
    CEGUI::Combobox * res_combobox_;
    CEGUI::Checkbox * fullscreen_checkbox_;
    CEGUI::Slider * sound_volume_slider_;
    CEGUI::Checkbox * music_checkbox_;
    CEGUI::Checkbox * shadows_enabled_checkbox_;
    CEGUI::Combobox * shadows_quality_combobox_;
    CEGUI::Combobox * fsaa_combobox_;
    CEGUI::Combobox * tex_quality_combobox_;
    CEGUI::Combobox * shader_quality_combobox_;
    CEGUI::Combobox * anisotropic_filtering_combobox_;
    CEGUI::Combobox * terrain_view_combobox_;
    CEGUI::Combobox * water_quality_combobox_;
    CEGUI::Slider * view_distance_slider_;
    CEGUI::Slider * grass_density_slider_;
    CEGUI::Combobox * sound_device_combobox_;

    MainMenu * main_menu_;

    LocalParameters keymap_params_;

    KeyMapElement * capturing_input_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class ResolutionListboxItem : public CEGUI::ListboxTextItem
{
public:
    ResolutionListboxItem(const CEGUI::String& text, CEGUI::uint item_id, unsigned width, unsigned height) : 
        ListboxTextItem(text, item_id),
        width_(width),
        height_(height)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned width_;
    unsigned height_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class SoundDeviceListboxItem : public CEGUI::ListboxTextItem
{
public:
    SoundDeviceListboxItem(const CEGUI::String& text, CEGUI::uint item_id, unsigned device_index) : 
        ListboxTextItem(text, item_id),
        device_index_(device_index)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned device_index_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class ShadowMapListboxItem : public CEGUI::ListboxTextItem
{
public:
    ShadowMapListboxItem(const CEGUI::String& text, CEGUI::uint item_id, unsigned shadow_map_size) : 
        ListboxTextItem(text, item_id),
        shadow_map_size_(shadow_map_size)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned shadow_map_size_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class FSAAListboxItem : public CEGUI::ListboxTextItem
{
public:
    FSAAListboxItem(const CEGUI::String& text, CEGUI::uint item_id, unsigned fsaa_samples) : 
        ListboxTextItem(text, item_id),
        fsaa_samples_(fsaa_samples)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned fsaa_samples_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class AFListboxItem : public CEGUI::ListboxTextItem
{
public:
    AFListboxItem(const CEGUI::String& text, CEGUI::uint item_id, unsigned af_samples) : 
        ListboxTextItem(text, item_id),
        af_samples_(af_samples)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned af_samples_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class TexQualityListboxItem : public CEGUI::ListboxTextItem
{
public:
    TexQualityListboxItem(const CEGUI::String& text, CEGUI::uint item_id, unsigned tex_quality) : 
        ListboxTextItem(text, item_id),
        tex_quality_(tex_quality)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned tex_quality_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class ShaderQualityListboxItem : public CEGUI::ListboxTextItem
{
public:
    ShaderQualityListboxItem(const CEGUI::String& text, CEGUI::uint item_id, unsigned shader_quality) : 
        ListboxTextItem(text, item_id),
        shader_quality_(shader_quality)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned shader_quality_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class WaterQualityListboxItem : public CEGUI::ListboxTextItem
{
public:
    WaterQualityListboxItem(const CEGUI::String& text, CEGUI::uint item_id, unsigned water_quality) : 
        ListboxTextItem(text, item_id),
        water_quality_(water_quality)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned water_quality_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxTextItem that auto-sets the selection brush
 *      image, color, font and so on. This saves doing it manually every time in the code.
 *      Also holds item specific values.
 **/
class TerrainViewListboxItem : public CEGUI::ListboxTextItem
{
public:
    TerrainViewListboxItem(const CEGUI::String& text, CEGUI::uint item_id,
                           unsigned start_grid,
                           unsigned num_grids,
                           unsigned grid_size) : 
        ListboxTextItem(text, item_id),
        start_grid_(start_grid),
        num_grids_(num_grids),
        grid_size_(grid_size)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }

    unsigned start_grid_;
    unsigned num_grids_;
    unsigned grid_size_;
};

//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxItemWindow that represents a  
 *          Key Name/Value Element inside a listbox
 **/
class KeyMapElement : public CEGUI::ListboxTextItem
{
public:
    KeyMapElement(const std::string & text, const std::string & prefix) :
        ListboxTextItem(text),
        prefix_(prefix)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont               (LISTBOX_ITEM_FONT);
        setSelectionColours   (LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours        (LISTBOX_ITEM_TEXT_COLOR);
    }

    std::string prefix_; ///< The action number prefix for ordering in the list.
};

#endif
