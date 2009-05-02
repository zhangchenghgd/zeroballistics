
#ifndef TANK_GUIPROFILER_INCLUDED
#define TANK_GUIPROFILER_INCLUDED

#include <CEGUI/CEGUI.h>

#include "RegisteredFpGroup.h"




//------------------------------------------------------------------------------
class GUIProfiler
{
 public:
    GUIProfiler(CEGUI::Window * parent);
    virtual ~GUIProfiler();
    
    void toggle();
    bool isActive() const;
    void refresh(float dt);
    
 protected:

    bool closeClicked(const CEGUI::EventArgs& e);
    bool onKeyDown(const CEGUI::EventArgs& e);
    bool onMouseDown(const CEGUI::EventArgs& e);

    CEGUI::FrameWindow * profiler_window_;
    CEGUI::Window * profiler_text_;

    RegisteredFpGroup fp_group_;
};


#endif
