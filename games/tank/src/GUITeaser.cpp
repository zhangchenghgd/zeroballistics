
#include "GUITeaser.h"


#include "MainMenu.h"
#include "SdlApp.h"


//------------------------------------------------------------------------------
GUITeaser::GUITeaser(MainMenu * main_menu) :
    main_menu_(main_menu)
{
    enableFloatingPointExceptions(false);

    loadWidgets();

    registerCallbacks();

    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
GUITeaser::~GUITeaser()
{
}

//------------------------------------------------------------------------------
void GUITeaser::show()
{
    teaser_window_->setVisible(true);
    teaser_window_->activate();
}

//------------------------------------------------------------------------------
void GUITeaser::loadWidgets()
{
    CEGUI::ImagesetManager::getSingleton().createImageset("Teaser.imageset","imagesets");

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("MainMenu_root/");

    if(!parent)
    {
        throw Exception("GUIHostMenu is missing the root window.");
    }
    
    // Use parent window name as prefix to avoid name clashes
    wm.loadWindowLayout("teaser.layout", parent->getName());
    teaser_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "teaser/image");


    // add teaser to widget tree
    parent->addChildWindow(teaser_window_);

    teaser_window_->setVisible(false);
}

//------------------------------------------------------------------------------
/**
*  register callbacks for actions
**/
void GUITeaser::registerCallbacks()
{
    teaser_window_->subscribeEvent(CEGUI::Window::EventMouseClick, CEGUI::Event::Subscriber(&GUITeaser::clickedScreen, this));    
    teaser_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUITeaser::clickedScreen, this));
}

//------------------------------------------------------------------------------
bool GUITeaser::clickedScreen(const CEGUI::EventArgs& e)
{
    s_app.quit();
    return true;
}
