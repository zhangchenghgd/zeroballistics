
#ifndef TANK_GAME_HUD_TANK_SOCCER_DEFINED
#define TANK_GAME_HUD_TANK_SOCCER_DEFINED

#include "GameHudTank.h"


//------------------------------------------------------------------------------
class GameHudTankSoccer : public GameHudTank
{
 public:
    GameHudTankSoccer(PuppetMasterClient * master, const std::string & config_file);
    virtual ~GameHudTankSoccer();

    virtual void onWindowResized();    

    virtual void activateHitFeedback() {}
    
 protected:

    virtual void update(float dt);

    
    osg::ref_ptr<HudBar> health_bar_;
    osg::ref_ptr<HudBar> reload_bar_;

    osg::ref_ptr<HudBar> reload_bar_skill2_;

    osg::ref_ptr<HudTextElement> time_limit_;
    osg::ref_ptr<HudTextElement> score_;
};


#endif
