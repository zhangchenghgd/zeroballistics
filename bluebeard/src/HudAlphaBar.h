
#ifndef TANK_ALPHA_HUD_BAR_INCLUDED
#define TANK_ALPHA_HUD_BAR_INCLUDED



#include "HudTextureElement.h"

//------------------------------------------------------------------------------
class HudAlphaBar : public HudTextureElement
{
 public:

    HudAlphaBar(const std::string & section);

    void setValue(float v);
    float getValue();
    
    virtual const char * className () const { return "HudAlphaBar"; }
    
 protected:

    float value_;
};



#endif
