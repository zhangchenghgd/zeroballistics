
#include "GUITeamSelect.h"


#include "Player.h"
#include "PuppetMasterClient.h"
#include "SdlApp.h"
#include "ParameterManager.h"
#include "GameLogicClientCommon.h"
#include "Team.h"
#include "Gui.h"
#include "InputHandler.h"

//------------------------------------------------------------------------------
GUITeamSelect::GUITeamSelect(PuppetMasterClient * puppet_master) :
    puppet_master_(puppet_master)
{
    enableFloatingPointExceptions(false);

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("TankApp_root/");
    
    // Use parent window name as prefix to avoid name clashes
    root_ = wm.loadWindowLayout("teamselectmenu.layout", parent->getName());
    menu_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "teamselectmenu/menu");
    menu_btn_[0] = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "teamselectmenu/btn1");
    menu_btn_[1] = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "teamselectmenu/btn2");
    menu_btn_spec_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "teamselectmenu/btn3");
    menu_btn_text_[0] = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "teamselectmenu/btn1/text");
    menu_btn_text_[1] = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "teamselectmenu/btn2/text");


    // add score to widget tree
    parent->addChildWindow(menu_window_);

    menu_window_->setVisible(false);

    registerCallbacks();

    enableFloatingPointExceptions();


    s_input_handler.registerInputCallback("Change Team",
                                          input_handler::SingleInputHandler(this, &GUITeamSelect::toggleShow),
                                          &fp_group_);
}

//------------------------------------------------------------------------------
GUITeamSelect::~GUITeamSelect()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();
    wm.destroyWindow(root_);
}

//------------------------------------------------------------------------------
void GUITeamSelect::registerCallbacks()
{
    menu_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUITeamSelect::onKeySelect, this));
    menu_window_->subscribeEvent(CEGUI::Window::EventMouseButtonDown, CEGUI::Event::Subscriber(&GUITeamSelect::clickedWindow, this));
    menu_btn_[0]->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUITeamSelect::clickedBtn1, this));
    menu_btn_[1]->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUITeamSelect::clickedBtn2, this));
    menu_btn_spec_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUITeamSelect::clickedBtn3, this));

    s_gui.subscribeActivationEvents(menu_window_);
}

//------------------------------------------------------------------------------
void GUITeamSelect::show(bool show)
{
    if (!menu_window_) return;

    menu_window_->setVisible(show);
    
    show ? menu_window_->activate() : menu_window_->deactivate();

//    if (show) clickedBtn1(CEGUI::EventArgs()); // development shortcut to game...
}

//------------------------------------------------------------------------------
void GUITeamSelect::setTeams(const std::vector<Team*> & teams)
{

    if (teams.size() == 1)
    {
        // enable first button
        menu_btn_[0]->setVisible(true);
        menu_btn_text_[0]->setVisible(true);
        menu_btn_text_[0]->setText(menu_btn_text_[0]->getText() + " Join");

        // get color representation
        Color color = teams[0]->getColor();
        std::string hex_col = color.getHexString(true);
        std::string cegui_hex_color = "tl:"+hex_col+" tr:"+hex_col+" bl:"+hex_col+" br:"+hex_col;

        menu_btn_text_[0]->setProperty("TextColours", cegui_hex_color);

        // disable second button
        menu_btn_[1]->setVisible(false);
        menu_btn_text_[1]->setVisible(false);

    }
    else if (teams.size() == 2)
    {
        for(unsigned t=0; t < teams.size(); t++)
        {
            menu_btn_[t]->setVisible(true);
            menu_btn_text_[t]->setVisible(true);
            menu_btn_text_[t]->setText(menu_btn_text_[t]->getText() +
                                         teams[t]->getName());

            // get color representation
            Color color = teams[t]->getColor();
            std::string hex_col = color.getHexString(true);
            std::string cegui_hex_color = "tl:"+hex_col+" tr:"+hex_col+" bl:"+hex_col+" br:"+hex_col;

            menu_btn_text_[t]->setProperty("TextColours",cegui_hex_color);
        }
    }
    else
    {
        s_log << Log::warning << "GUITeamSelect cannot handle Team size: " << teams.size() << "\n";
    }



}

//------------------------------------------------------------------------------
void GUITeamSelect::toggleShow()
{
    show(!menu_window_->isVisible());
}

//------------------------------------------------------------------------------
bool GUITeamSelect::onKeySelect(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
        case CEGUI::Key::One:
            clickedBtn1(e);
            return true;
        case CEGUI::Key::Two:
            clickedBtn2(e);
            return true;
        case CEGUI::Key::Three:
            clickedBtn3(e);
            return true;
        default:
            return false;
    }    
}

//------------------------------------------------------------------------------
bool GUITeamSelect::clickedBtn1(const CEGUI::EventArgs& e)
{
    GameLogicClientCommon * glcc = (GameLogicClientCommon*)puppet_master_->getGameLogic();
    if(glcc)
    {
        glcc->sendTeamChangeRequest(0);
        toggleShow();
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
bool GUITeamSelect::clickedBtn2(const CEGUI::EventArgs& e)
{
    GameLogicClientCommon * glcc = (GameLogicClientCommon*)puppet_master_->getGameLogic();
    if(glcc)
    {
        glcc->sendTeamChangeRequest(1);
        toggleShow();
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
bool GUITeamSelect::clickedBtn3(const CEGUI::EventArgs& e)
{
    GameLogicClientCommon * glcc = (GameLogicClientCommon*)puppet_master_->getGameLogic();
    if(glcc)
    {
        glcc->sendTeamChangeRequest(INVALID_TEAM_ID);
        toggleShow();
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
bool GUITeamSelect::clickedWindow(const CEGUI::EventArgs& e)
{
    // just consume the click event inside window
    return true;
}
