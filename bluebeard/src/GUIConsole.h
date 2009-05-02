
#ifndef TANK_GUICONSOLE_INCLUDED
#define TANK_GUICONSOLE_INCLUDED


#include <string>
#include <vector>

#include <CEGUI/CEGUI.h>


#include "RegisteredFpGroup.h"

namespace CEGUI
{
    class Window;
}

//------------------------------------------------------------------------------
class GUIConsole
{
 public:
    GUIConsole(CEGUI::Window * parent);
    virtual ~GUIConsole();

    void registerCallbacks();
    bool isActive() const;
    
    void addOutputString(const std::string & str);
    
    void toggleShow();
    
 protected:

    void addOutputLine(const std::string & line);
    
    bool onKeyDown(const CEGUI::EventArgs& e);
    bool renderStart(const CEGUI::EventArgs& e);

    void histUp();
    void histDown();
    void autoComplete();

    std::string cur_line_; ///< The line currently being built by addOutputString.

    bool console_redraw_;

    CEGUI::Window * console_window_;
    CEGUI::Editbox * editbox_;
    CEGUI::MultiLineEditbox * display_;

    CEGUI::String accum_console_text_;

    RegisteredFpGroup fp_group_;
};


#endif
