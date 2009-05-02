
#include "GUIHelp.h"

#include "Gui.h"
#include "Utils.h"
#include "SdlApp.h"
#include "InputHandler.h"


//------------------------------------------------------------------------------
GUIHelp::GUIHelp()
{
    enableFloatingPointExceptions(false);

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("TankApp_root/");

    if(!parent)
    {
        throw Exception("GUIHelp is missing the root window. XXX hack!");
    }
    
    // Use parent window name as prefix to avoid name clashes
    root_ = wm.loadWindowLayout("help.layout", parent->getName());
    help_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "help/window");
    tab1_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "help/tab1_content");
    tab1_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "help/tab1_btn");
    close_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "help/window/close_btn");

    if (!help_window_)
    {
        throw Exception("GUI Help screen is missing a widget!");
    }

    // add score to widget tree
    parent->addChildWindow(help_window_);

    help_window_->setVisible(false);
    tab1_window_->setVisible(true);

    registerCallbacks();

    enableFloatingPointExceptions();


    s_input_handler.registerInputCallback("Show Help",
                                          input_handler::SingleInputHandler(this, &GUIHelp::toggleShow),
                                          &fp_group_);
}

//------------------------------------------------------------------------------
GUIHelp::~GUIHelp()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();
    wm.destroyWindow(root_);
}




//------------------------------------------------------------------------------
bool GUIHelp::clickedTab1Btn(const CEGUI::EventArgs& e)
{   
    tab1_window_->setVisible(true);
    return true;
}

//------------------------------------------------------------------------------
bool GUIHelp::clickedCloseBtn(const CEGUI::EventArgs& e)
{
    toggleShow();
    return true;
}

//------------------------------------------------------------------------------
void GUIHelp::toggleShow()
{
    bool show = !help_window_->isVisible();
    help_window_->setVisible(show);
}


//------------------------------------------------------------------------------
void GUIHelp::registerCallbacks()
{
    tab1_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIHelp::clickedTab1Btn, this));
    close_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIHelp::clickedCloseBtn, this));

    s_gui.subscribeActivationEvents(help_window_);
}
