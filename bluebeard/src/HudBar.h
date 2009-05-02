
#ifndef TANK_HUD_BAR_INCLUDED
#define TANK_HUD_BAR_INCLUDED



#include "HudTextureElement.h"

//------------------------------------------------------------------------------
class HudBar : public HudTextureElement
{
 public:

    HudBar(bool vertical, const std::string & section);

    void setValue(float v);
    float getValue();
    
    virtual const char * className () const { return "HudBar"; }
    
 protected:

    virtual void updateVb();
    

    float value_;

    bool vertical_;
};



#endif
