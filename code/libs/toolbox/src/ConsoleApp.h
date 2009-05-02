#ifndef BLUEBEARD_CONSOLE_APP_INCLUDED
#define BLUEBEARD_CONSOLE_APP_INCLUDED



#include <string>
#include <vector>

#include "Datatypes.h"
#include "RegisteredFpGroup.h"

//------------------------------------------------------------------------------
class ConsoleApp
{
 public:
    ConsoleApp();
    virtual ~ConsoleApp();

    void run();
    
 protected:

#ifdef _WIN32
    static BOOL WINAPI ConsoleHandler(DWORD CEvent);
#endif

    std::string quit(const std::vector<std::string>&);

    void sleep(int msecs);
    void handleInput();
    void handleInputWin32();

    int setFlag(int fd, int flags);
    
    void printCmdLine(unsigned cursor_pos, const std::string & cmd_line);
    void deleteChar(unsigned & cursor_pos, std::string & cmd_line);

    std::string cmd_line_;
    unsigned cursor_pos_;
    
    bool dumb_terminal_;
    bool quit_;

    RegisteredFpGroup fp_group_;
};

#endif
