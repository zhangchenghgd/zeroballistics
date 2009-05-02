

#ifndef BLUEBEARD_GUI_LOGGER_INCLUDED
#define BLUEBEARD_GUI_LOGGER_INCLUDED

#include <CEGUI/CEGUILogger.h>


//------------------------------------------------------------------------------
/**
 *  We need our own logger because CEGUI's logger throws if it cannot
 *  find the logfile.
 */
class GuiLogger : public CEGUI::Logger
{
public:

    GuiLogger(void);
    virtual ~GuiLogger(void);

    virtual void logEvent(const CEGUI::String& message, CEGUI::LoggingLevel level = CEGUI::Standard);
    virtual void setLogFilename(const CEGUI::String& filename, bool append = false);

};


#endif
