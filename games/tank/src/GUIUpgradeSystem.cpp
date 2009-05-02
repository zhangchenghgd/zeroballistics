
#include "GUIUpgradeSystem.h"

#include "Utils.h"
#include "Player.h"
#include "PuppetMasterClient.h"
#include "ParameterManager.h"
#include "SdlApp.h"
#include "InputHandler.h"
#include "Gui.h"

#include "GameLogicClientCommon.h"


//------------------------------------------------------------------------------
GUIUpgradeSystem::GUIUpgradeSystem(PuppetMasterClient * puppet_master) :
            puppet_master_(puppet_master)
{
    enableFloatingPointExceptions(false);

    loadWidgets();

    registerCallbacks();

    enableFloatingPointExceptions();


    s_input_handler.registerInputCallback("Show Upgrade Screen",
                                          input_handler::SingleInputHandler(this, &GUIUpgradeSystem::toggleShow),
                                          &fp_group_);
}

//------------------------------------------------------------------------------
GUIUpgradeSystem::~GUIUpgradeSystem()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();
    wm.destroyWindow(root_);
}

//------------------------------------------------------------------------------
void GUIUpgradeSystem::update(const Score & score)
{
    const PlayerScore * local_score = score.getPlayerScore(puppet_master_->getLocalPlayer()->getId());
    assert(local_score);

    upgrade_points_label_->setText(toString(local_score->upgrade_points_));
            
    for(uint8_t cat = 0; cat < NUM_UPGRADE_CATEGORIES; cat++)
    {
        // Only show upgrade button if upgrade can be done.
        button_[cat]->setEnabled(local_score->isUpgradePossible((UPGRADE_CATEGORY)cat));

        // check selected upgrades
        for(uint8_t lvl = 0; lvl < 3; lvl++)
        {
            active_upgrades_[cat][lvl]->setSelected(local_score->active_upgrades_[cat] > lvl);
        }
    }
}

//------------------------------------------------------------------------------
void GUIUpgradeSystem::loadWidgets()
{

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("TankApp_root/");
    
    // Use parent window name as prefix to avoid name clashes
    root_ = wm.loadWindowLayout("upgradesystem.layout", parent->getName());
    upgrade_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "upgrade/window");
    upgrade_points_label_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "upgrade/label_points");
    close_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "upgrade/window/close_btn");

    button_[0] = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "upgrade/btn1");
    button_[1] = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "upgrade/btn2");
    button_[2] = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "upgrade/btn3");


    std::string label = ""; // repr. upgrade points + description
    for(unsigned cat=0; cat < NUM_UPGRADE_CATEGORIES; cat++)
    for(unsigned lvl=0; lvl < 3; lvl++)
    {
        active_upgrades_[cat][lvl] = (CEGUI::Checkbox*) wm.getWindow(parent->getName() +
                        "upgrade/" + UPGRADES[cat] + toString((lvl+1)) + "_activation");

        upgrades_desc_[cat][lvl] = (CEGUI::Window*) wm.getWindow(parent->getName() +
                        "upgrade/" + UPGRADES[cat] + toString((lvl+1)) + "_text");

        label = toString(s_params.get<unsigned>("upgrade.cost." + UPGRADES[cat] + toString((lvl+1)))) + " UP";
        label += "\n" + s_params.get<std::string>("upgrade.desc." + UPGRADES[cat] + toString((lvl+1)));
        upgrades_desc_[cat][lvl]->setText(label);

        active_upgrades_[cat][lvl]->setEnabled(false);
    }

    // add window to widget tree
    parent->addChildWindow(upgrade_window_);

    upgrade_window_->setVisible(false);

}

//------------------------------------------------------------------------------
void GUIUpgradeSystem::toggleShow()
{
    bool show = !upgrade_window_->isVisible();
    upgrade_window_->setVisible(show);

    if(show)
    {
        close_btn_->activate();
        upgrade_window_->activate();
    }
    else
    {
        upgrade_window_->deactivate();
    }
}


//------------------------------------------------------------------------------
void GUIUpgradeSystem::registerCallbacks()
{
    upgrade_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUIUpgradeSystem::onKeySelect, this));
    close_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIUpgradeSystem::clickedCloseBtn, this));

    button_[0]->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIUpgradeSystem::clickedButton1, this));
    button_[1]->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIUpgradeSystem::clickedButton2, this));
    button_[2]->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIUpgradeSystem::clickedButton3, this));


    s_gui.subscribeActivationEvents(upgrade_window_);
}

//------------------------------------------------------------------------------
bool GUIUpgradeSystem::clickedButton1(const CEGUI::EventArgs& e)
{   
    GameLogicClientCommon * glcc = (GameLogicClientCommon*)puppet_master_->getGameLogic();
    glcc->sendUpgradeRequest(UC_WEAPON);

    toggleShow();
    return true;
}

//------------------------------------------------------------------------------
bool GUIUpgradeSystem::clickedButton2(const CEGUI::EventArgs& e)
{   
    GameLogicClientCommon * glcc = (GameLogicClientCommon*)puppet_master_->getGameLogic();
    glcc->sendUpgradeRequest(UC_ARMOR);

    toggleShow();
    return true;
}

//------------------------------------------------------------------------------
bool GUIUpgradeSystem::clickedButton3(const CEGUI::EventArgs& e)
{   
    GameLogicClientCommon * glcc = (GameLogicClientCommon*)puppet_master_->getGameLogic();
    glcc->sendUpgradeRequest(UC_SPEED);

    toggleShow();
    return true;
}

//------------------------------------------------------------------------------
bool GUIUpgradeSystem::clickedCloseBtn(const CEGUI::EventArgs& e)
{
    toggleShow();
    return true;
}

//------------------------------------------------------------------------------
bool GUIUpgradeSystem::onKeySelect(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
        case CEGUI::Key::One:
            clickedButton1(e);
            return true;
        case CEGUI::Key::Two:
            clickedButton2(e);
            return true;
        case CEGUI::Key::Three:
            clickedButton3(e);
            return true;
        default:
            return false;
    }    
}
