

#include "GuiLogger.h"


#include "Log.h"

//------------------------------------------------------------------------------
GuiLogger::GuiLogger(void)
{
}

//------------------------------------------------------------------------------
GuiLogger::~GuiLogger(void)
{
}



//------------------------------------------------------------------------------
void GuiLogger::logEvent(const CEGUI::String& message, CEGUI::LoggingLevel level)
{
    switch (level)
    {
    case CEGUI::Errors:
        s_log << Log::error
              << "CEGUI: "
              << message.c_str()
              << "\n";
        break;
    default:
        s_log << Log::debug('C')
              << message.c_str()
              << "\n";
    }
}


//------------------------------------------------------------------------------
void GuiLogger::setLogFilename(const CEGUI::String& filename, bool append)
{
}
