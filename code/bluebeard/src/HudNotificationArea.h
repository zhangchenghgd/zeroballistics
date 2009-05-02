

#ifndef HUD_NOTIFICATION_AREA_INCLUDED
#define HUD_NOTIFICATION_AREA_INCLUDED

#include <vector>

#include <osg/ref_ptr>

#include "Datatypes.h"
#include "Vector2d.h"



namespace osg
{
    class Geode;
}

class HudTextElement;

//------------------------------------------------------------------------------
class HudNotificationArea
{
 public:
    HudNotificationArea(const std::string & section,
                        osg::Geode * geode);
    virtual ~HudNotificationArea();

    void clear();
    void addLine(const std::string & msg, const Color & color = Color(1.0f,1.0f,1.0f));
    void appendToLine(const std::string & msg);

    void recalcTextPos();
 protected:

    unsigned last_line_; ///< Index of the last line that was displayed
    unsigned last_nonempty_line_;
    
    std::vector<osg::ref_ptr<HudTextElement> > label_;

    std::string section_;
};


#endif
