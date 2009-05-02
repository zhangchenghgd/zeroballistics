
#include "GUIConsole.h"

#include <limits>

#include "Console.h"
#include "Gui.h"
#include "InputHandler.h"
#include "SdlApp.h"
#include "SdlKeyNames.h"

const unsigned MAX_CONSOLE_HISTORY = 5000;

const std::string SHOW_CONSOLE_NAME = "Show Console";

//------------------------------------------------------------------------------
GUIConsole::GUIConsole(CEGUI::Window * parent) :
            console_redraw_(false),
            console_window_(NULL),
            editbox_(NULL),
            display_(NULL)
{
    enableFloatingPointExceptions(false);
    
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    // Use parent window name as prefix to avoid name clashes
    wm.loadWindowLayout("console.layout", parent->getName());
    console_window_ = (CEGUI::Window*)           wm.getWindow(parent->getName() + "console/window");
    display_        = (CEGUI::MultiLineEditbox*) wm.getWindow(parent->getName() + "console/display");
    editbox_        = (CEGUI::Editbox*)          wm.getWindow(parent->getName() + "console/editbox");

    if (!console_window_ || !display_ || !editbox_)
    {
        throw Exception("GUI Console is missing a widget!");
    }

    // add console to widget tree
    parent->addChildWindow(console_window_);

    registerCallbacks();

    console_window_->setVisible(false);
    console_window_->setEnabled(false);


    s_console.setPrintCallback(PrintCallback(this, &GUIConsole::addOutputString));


    enableFloatingPointExceptions();


    s_input_handler.registerInputCallback(SHOW_CONSOLE_NAME,
                                          input_handler::SingleInputHandler(this, &GUIConsole::toggleShow),
                                          &fp_group_);
}

//------------------------------------------------------------------------------
GUIConsole::~GUIConsole()
{
    s_console.setPrintCallback(PrintCallback(&Console::dummyPrint));

    // needed to remove window from activated stack in s_gui if left over
    console_window_->setVisible(false);
}

//------------------------------------------------------------------------------
void GUIConsole::registerCallbacks()
{
    s_gui.subscribeActivationEvents(console_window_);

    editbox_->subscribeEvent(CEGUI::Editbox::EventKeyDown, CEGUI::Event::Subscriber(&GUIConsole::onKeyDown, this));
    display_->subscribeEvent(CEGUI::MultiLineEditbox::EventRenderingStarted, CEGUI::Event::Subscriber(&GUIConsole::renderStart, this));

}



//------------------------------------------------------------------------------
bool GUIConsole::isActive() const
{
    if (!console_window_) return false;
    return console_window_->isVisible();
}

//------------------------------------------------------------------------------
/**
 *  Accumulates output strings in cur_line_. Complete lines are passed
 *  to addOutputLine.
 */
void GUIConsole::addOutputString(const std::string & str)
{  
    cur_line_ += str;
    
    for(;;)
    {
        std::string::size_type nl_pos = cur_line_.find('\n');
        if (nl_pos == std::string::npos) return;
        
        addOutputLine(cur_line_.substr(0, nl_pos));
        cur_line_ = cur_line_.substr(nl_pos + 1);
    }
}


//------------------------------------------------------------------------------
void GUIConsole::toggleShow()
{
    if (!console_window_) return;
    console_window_->setVisible(!console_window_->isVisible());

    if (console_window_->isVisible()) 
    {
        console_window_->enable();
        display_->activate();
        editbox_->enable();
        editbox_->activate();

        s_app.captureMouse(false);
    }
    else
    {
        console_window_->disable();
        display_->deactivate();
        editbox_->deactivate();
        editbox_->disable();
    }
}


//------------------------------------------------------------------------------
void GUIConsole::addOutputLine(const std::string & l)
{
    using namespace std;
    string line = l;
      
    // Replace all tabs with 3 spaces
    string::size_type tab;
    while ((tab = line.find('\t')) != string::npos)
    {
        line = line.replace(tab, 1, "   ");
    }

    // add new output text
    accum_console_text_.append(line + "\n");

    // cut console text down to max history size
    if(accum_console_text_.size() > MAX_CONSOLE_HISTORY)
    {
        accum_console_text_ = accum_console_text_.substr( (accum_console_text_.size())-MAX_CONSOLE_HISTORY,
                                                          MAX_CONSOLE_HISTORY);
    }

    console_redraw_ = true;
    console_window_->requestRedraw(); 
}

//------------------------------------------------------------------------------
bool GUIConsole::onKeyDown(const CEGUI::EventArgs& e)
{
    CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;
    std::string input_string;
    std::string result;

    // See whether the key should close the console...
    if (s_input_handler.getKeyForFunction(SHOW_CONSOLE_NAME) ==
        input_handler::getKeyWithCode(s_gui.CEGUIKeyToSDLKey(ek->scancode)).name_)
    {
        ek->handled = true;
        toggleShow();
        return true;
    }

    switch(ek->scancode)
    {
        case CEGUI::Key::Return:
            ek->handled = true;
            input_string = editbox_->getText().c_str();
            result = s_console.executeCommand(input_string.c_str());

            editbox_->setText("");
            
            if (!result.empty())
            {
                addOutputString(result + "\n");
            } else
            {
                addOutputString(input_string + "\n");
            }
            return true;
        case CEGUI::Key::ArrowUp:
            ek->handled = true;
            histUp();
            return true;
        case CEGUI::Key::ArrowDown:
            ek->handled = true;
            histDown();
            return true;
        case CEGUI::Key::Tab:
            ek->handled = true;
            autoComplete();
            return false;
        default:
            return false;
    }

    return false;
}

//------------------------------------------------------------------------------
bool GUIConsole::renderStart(const CEGUI::EventArgs& e)
{

    if(console_redraw_)
    {
      display_->setText(accum_console_text_);

      display_->setCaratIndex(static_cast<size_t>(-1));

      console_redraw_ = false;
    } 

    return true;
}


//------------------------------------------------------------------------------
void GUIConsole::histUp()
{
    std::string next = s_console.getNextHistItem();
    editbox_->setText(next);
    editbox_->setCaratIndex(next.size());
}


//------------------------------------------------------------------------------
void GUIConsole::histDown()
{
    std::string prev = s_console.getPrevHistItem();
    editbox_->setText(prev);
    editbox_->setCaratIndex(prev.size());
}

//------------------------------------------------------------------------------
void GUIConsole::autoComplete()
{
    std::vector<std::string> completions;
    
    std::string partial_input = editbox_->getText().c_str();
    completions = s_console.getCompletions(partial_input);


    editbox_->setText(partial_input);
    editbox_->setCaratIndex(partial_input.size());
    
    if (completions.empty()) return;
    
    if (completions.size() > 1)
    {
        addOutputLine(" ");
        for (unsigned i=0; i<completions.size(); ++i)
        {
            addOutputLine(completions[i]);
        }
    }
}
