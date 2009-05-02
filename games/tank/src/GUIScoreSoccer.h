
#ifndef TANK_GUISCORESOCCER_INCLUDED
#define TANK_GUISCORESOCCER_INCLUDED

#include "GUIScore.h" 

class PuppetMasterClient;

//------------------------------------------------------------------------------
class GUIScoreSoccer : public GUIScore
{
 public:
    GUIScoreSoccer(PuppetMasterClient * puppet_master);
    virtual ~GUIScoreSoccer();

    virtual void update(const Score & score);    
    
 protected:

    virtual void loadWidgets();
    virtual void customizeLayout();

    CEGUI::Window * acc_label_;
    CEGUI::Window * eff_label_;

};


#endif
