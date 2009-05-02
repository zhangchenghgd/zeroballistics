
#ifndef TANK_HUD_TEXT_ELEMENT_INCLUDED
#define TANK_HUD_TEXT_ELEMENT_INCLUDED



#include <osgText/Text>

//------------------------------------------------------------------------------
class HudTextElement : public osgText::Text
{
 public:

    HudTextElement(const std::string & section);
    
    void recalcTextPos();

 protected:
    std::string section_;
};



#endif
