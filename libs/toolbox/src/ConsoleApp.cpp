
#include "ConsoleApp.h"


#include "ParameterManager.h"
#include "TimeStructs.h"
#include "Scheduler.h"
#include "VariableWatcher.h"
#include "Log.h"

#ifdef _WIN32
#include <conio.h>
#include <stdio.h>
#include <wincon.h>
#else
#include <unistd.h>
#endif

#include <limits>
#include <fcntl.h>

#undef min
#undef max

#ifndef _WIN32

const int VK_BACKSPACE = 0x007f;
const int VK_TAB       = 0x0009;
const int VK_RETURN    = 0x000d;
const int VK_DELETE    = 0xff33;

const int VK_UP        = 0xff41;
const int VK_DOWN      = 0xff42;
const int VK_RIGHT     = 0xff43;
const int VK_LEFT      = 0xff44;

const int VK_END       = 0xff46;
const int VK_HOME      = 0xff48;


const int VK_CTRL_A    = 0x0001;
const int VK_CTRL_C    = 0x0003;
const int VK_CTRL_D    = 0x0004;
const int VK_CTRL_E    = 0x0005;
const int VK_CTRL_K    = 0x000b;
const int VK_CTRL_L    = 0x000c;
const int VK_CTRL_Q    = 0x0011;


// http://www.linuxselfhelp.com/howtos/Bash-Prompt/Bash-Prompt-HOWTO-6.html
const char * CTRL_ERASE_LINE   = "\033[K";
const char * CTRL_ERASE_SCREEN = "\033[2J";
const char * CTRL_RESET_CURSOR = "\033[0;0H";

#endif

#ifdef _WIN32
const int VK_ARROW_UP        = 256 + 72;
const int VK_ARROW_DOWN      = 256 + 80;
const int VK_ARROW_RIGHT     = 256 + 77;
const int VK_ARROW_LEFT      = 256 + 75;
#endif

//------------------------------------------------------------------------------
ConsoleApp::ConsoleApp() :
    cursor_pos_(0),
    dumb_terminal_(true),
    quit_(false)
{
#ifndef _WIN32
    // Check whether we have dumb terminal (e.g. started from ddd)
    char * term = getenv("TERM");
    if (term && strcmp(term, "dumb") != 0)
    {
        system("stty raw -echo -isig");
        dumb_terminal_ = false;
    } else
    {
        s_log << "Dumb terminal detected. Console input will be limited.\n";
    }
    setFlag(STDIN_FILENO, O_NONBLOCK);
#endif

    s_console.addFunction("quit", ConsoleFun(this, &ConsoleApp::quit), &fp_group_);
    s_console.addFunction("exit", ConsoleFun(this, &ConsoleApp::quit), &fp_group_); 

#ifdef _WIN32
    if (SetConsoleCtrlHandler(
        (PHANDLER_ROUTINE)&ConsoleApp::ConsoleHandler,TRUE)==FALSE)
    {
        // unable to install handler... display message to the user
        s_log << Log::error << "Unable to install console event handler!\n";
    }
#endif
}

//------------------------------------------------------------------------------
ConsoleApp::~ConsoleApp()
{

#ifndef _WIN32
    if (!dumb_terminal_) system("stty -raw echo");
#endif
}


//------------------------------------------------------------------------------
void ConsoleApp::run()
{
    TimeValue last_time;
    getCurTime(last_time);

    while (!quit_)
    {
        TimeValue cur_time;
        getCurTime(cur_time);
        float dt = getTimeDiff(cur_time, last_time) * 0.001f;
            
        // due to timer resolution
        if(dt == 0.0f) continue;
            
        last_time = cur_time;

        dt = std::min(dt, 1.0f / s_params.get<float>("server.app.min_fps"));
            
        s_scheduler.frameMove(dt);
            
#ifndef _WIN32
        handleInput();
#else          
        handleInputWin32();
#endif        

        getCurTime(cur_time);
        int sleep_time = (int)(1000.0f / s_params.get<float>("server.app.target_fps") -
                               getTimeDiff(cur_time, last_time));

        if (sleep_time > 0) sleep(sleep_time);
    }        
}


#ifdef _WIN32
//------------------------------------------------------------------------------
/**
 *  Registered event handler function to handle Win32 close events correctly
 *  otherwise program crashes on exit
 */
BOOL WINAPI ConsoleApp::ConsoleHandler(DWORD CEvent)
{
    switch(CEvent)
    {
    case CTRL_C_EVENT:
        s_log << "CTRL+C received!\n";
        break;
    case CTRL_BREAK_EVENT:
        s_log << "CTRL+BREAK received!\n";
        break;
    case CTRL_CLOSE_EVENT:
        s_log << "Program being closed!\n";
        break;
    case CTRL_LOGOFF_EVENT:
        s_log << "User is logging off!\n";
        break;
    case CTRL_SHUTDOWN_EVENT:
        s_log << "User is logging off!\n";
        break;
    }
    
    s_console.executeCommand("quit");

    return TRUE;
}
#endif


//------------------------------------------------------------------------------
/**
 *  Registered as console function to quit the app.
 */
std::string ConsoleApp::quit(const std::vector<std::string>&)
{
    quit_ = true;
    return "";
}


//------------------------------------------------------------------------------
void ConsoleApp::sleep(int msecs)
{
#ifdef _WIN32
    ::Sleep((DWORD)msecs);    
#else
    usleep(1000*msecs);
#endif    
}

//------------------------------------------------------------------------------
/**
 *  First see whether a key was pressed and return immediately if not.
 *
 *  
 */
void ConsoleApp::handleInput()
{
#ifndef _WIN32
    std::vector<std::string> completions;
    std::string ret;
    
    int ch = fgetc(stdin);
    if (ch == -1) return;

    // Handle special key sequences
    if (ch == 0x1b)
    {
        ch = fgetc(stdin);
        if (ch == 0x5b)
        {
            ch = fgetc(stdin) | 0xff00;
            fgetc(stdin);
        } else
        {
            ch = fgetc(stdin) | 0xff00;
        }
    }

    
//    std::cout << (void*) ch << "\n";
    switch (ch)
    {
    case VK_LEFT:
        if (cursor_pos_ != 0)
        {
            std::cout << "\033[1D";
            --cursor_pos_;
        }
        break;
    case VK_RIGHT:
        if (cursor_pos_ != cmd_line_.size())
        {
            std::cout << "\033[1C";
            ++cursor_pos_;
        }
        break;
        
    case VK_DOWN:
        cmd_line_ = s_console.getPrevHistItem();
        std::cout << "\r" << CTRL_ERASE_LINE << cmd_line_;
        cursor_pos_ = cmd_line_.size();
        break;
    case VK_UP:
        cmd_line_ = s_console.getNextHistItem();
        std::cout << "\r" << CTRL_ERASE_LINE << cmd_line_;
        cursor_pos_ = cmd_line_.size();
        break;
        
    case VK_BACKSPACE:
        if (cursor_pos_ != 0)
        {
            --cursor_pos_;
            std::cout << "\033[1D";
            deleteChar(cursor_pos_, cmd_line_);
        }
        break;
    case VK_RETURN:
    case '\n':
        ret = s_console.executeCommand(cmd_line_.c_str());

        if (!dumb_terminal_) std::cout << "\n\r";
        if (ret.size())
        {
            std::cout << addCr(ret)
                      << "\n\r";

            if (!std::cout)
            {
                std::cout.clear();
                s_log << Log::warning
                      << "std::cout cleared\n";
            }            
        }
            
        cmd_line_ = "";
        cursor_pos_ = 0;
        break;
    case VK_TAB:
        completions = s_console.getCompletions(cmd_line_);

        std::cout << "\r" << CTRL_ERASE_LINE;

        if (!completions.empty())
        {
            std::cout << "\n";
            for (unsigned c=0; c<completions.size(); ++c)
            {
                std::cout << completions[c]
                          << "\n\r";
            }
        }
            
        std::cout << cmd_line_;
        cursor_pos_ = cmd_line_.size();

        break;
    case VK_CTRL_A:
    case VK_HOME:
        std::cout << "\033[" << toString(cmd_line_.size()) << "D";
        cursor_pos_ = 0;
        break;
    case VK_CTRL_C:
        std::cout << "\r\n";
        cmd_line_ = "";
        cursor_pos_ = 0;
        break;
    case VK_CTRL_D:
    case VK_DELETE:
        deleteChar(cursor_pos_, cmd_line_);
        break;
    case VK_CTRL_E:
    case VK_END:
        std::cout << "\033[" << toString(cmd_line_.size() - cursor_pos_) << "C";
        cursor_pos_ = cmd_line_.size();
        break;
    case VK_CTRL_K:
        std::cout << CTRL_ERASE_LINE;
        cmd_line_.resize(cursor_pos_);
        break;
    case VK_CTRL_L:
        std::cout << CTRL_ERASE_SCREEN << CTRL_RESET_CURSOR;
        printCmdLine(cursor_pos_, cmd_line_);
        break;
    case VK_CTRL_Q:
        quit_ = true;
        break;
    default:
        cmd_line_.insert(cursor_pos_, 1, (char)ch);
        ++cursor_pos_;

        if (!dumb_terminal_) printCmdLine(cursor_pos_, cmd_line_);
    }
#endif
}



//------------------------------------------------------------------------------
/**
 *  First see whether a key was pressed and return immediately if not.
 */
void ConsoleApp::handleInputWin32()
{
#ifdef _WIN32
    std::vector<std::string> completions;
    std::string ret;
    HANDLE console_wnd = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO console_screen_info;
    LPDWORD written;
    char newl = '\n';
    int old_length = 0;

    if (_kbhit()) 
    {

        int ch = _getch();

        // handle special keys
        if ( ch == 0 || ch == 224 )
        {
            ch = 256 + _getch();
        }

        switch (ch)
        {
        case VK_HOME:
        case VK_DELETE:
        case VK_END:
            break;
        case VK_TAB:
            completions = s_console.getCompletions(cmd_line_);

            if (!completions.empty())
            {
                WriteConsole(console_wnd, &newl,  sizeof(newl),  written, NULL);
                for (unsigned c=0; c<completions.size(); ++c)
                {
                    WriteConsole(console_wnd, completions[c].c_str(), completions[c].size(),  written,  NULL);
                    WriteConsole(console_wnd, &newl,  sizeof(newl),  written, NULL);
                }
            }

            GetConsoleScreenBufferInfo(console_wnd, &console_screen_info);

            COORD curs_pos;
            curs_pos.X = 0;
            curs_pos.Y = console_screen_info.dwCursorPosition.Y;
            SetConsoleCursorPosition(console_wnd,  curs_pos);
            WriteConsole(console_wnd, cmd_line_.c_str(), cmd_line_.size(),  written,  NULL);

            cursor_pos_ = cmd_line_.size();

            break;
        case VK_ARROW_LEFT:
            if (cursor_pos_ != 0)
            {
                GetConsoleScreenBufferInfo(console_wnd, &console_screen_info);
                COORD new_pos = console_screen_info.dwCursorPosition;
                new_pos.X = new_pos.X - 1;
                SetConsoleCursorPosition(console_wnd, new_pos);
                --cursor_pos_;
            }
            break;
        case VK_ARROW_RIGHT:
            if (cursor_pos_ != cmd_line_.size())
            {
                GetConsoleScreenBufferInfo(console_wnd, &console_screen_info);
                COORD new_pos = console_screen_info.dwCursorPosition;
                new_pos.X = new_pos.X + 1;
                SetConsoleCursorPosition(console_wnd, new_pos);
                ++cursor_pos_;
            }
            break;
        case VK_ARROW_UP:
            old_length = cmd_line_.size();
            cmd_line_ = s_console.getPrevHistItem();

            // clear current line
            GetConsoleScreenBufferInfo(console_wnd, &console_screen_info);
            COORD new_pos;
            new_pos.X = 0;
            new_pos.Y = console_screen_info.dwCursorPosition.Y;
            FillConsoleOutputCharacter(console_wnd, ' ', old_length, new_pos, written);

            // reset to first col
            SetConsoleCursorPosition(console_wnd, new_pos);

            // write cmd
            WriteConsole(console_wnd, cmd_line_.c_str(), cmd_line_.size(),  written,  NULL);
            new_pos.X = cmd_line_.size();
            SetConsoleCursorPosition(console_wnd, new_pos);

            cursor_pos_ = cmd_line_.size();
            break;
        case VK_ARROW_DOWN:        
            old_length = cmd_line_.size();
            cmd_line_ = s_console.getNextHistItem();

            GetConsoleScreenBufferInfo(console_wnd, &console_screen_info);
            COORD new_pos2;
            new_pos2.X = 0;
            new_pos2.Y = console_screen_info.dwCursorPosition.Y;
            FillConsoleOutputCharacter(console_wnd, ' ', old_length, new_pos2, written);

            // reset to first col
            SetConsoleCursorPosition(console_wnd, new_pos2);

            // write cmd
            WriteConsole(console_wnd, cmd_line_.c_str(), cmd_line_.size(),  written,  NULL);
            new_pos2.X = cmd_line_.size();
            SetConsoleCursorPosition(console_wnd, new_pos2);


            cursor_pos_ = cmd_line_.size();        
            break;

        case VK_BACK:
            if (cursor_pos_ != 0)
            {
                --cursor_pos_;

                GetConsoleScreenBufferInfo(console_wnd, &console_screen_info);

                COORD curs_pos;
                curs_pos.X = console_screen_info.dwCursorPosition.X-1;
                curs_pos.Y = console_screen_info.dwCursorPosition.Y;
                SetConsoleCursorPosition(console_wnd,  curs_pos);

                char c = ' ';
                WriteConsole(console_wnd, &c,  sizeof(c),  written, NULL);

                GetConsoleScreenBufferInfo(console_wnd, &console_screen_info);
                
                curs_pos.X = console_screen_info.dwCursorPosition.X-1;
                curs_pos.Y = console_screen_info.dwCursorPosition.Y;
                SetConsoleCursorPosition(console_wnd,  curs_pos);

                // deleteChar
                if (cursor_pos_ != cmd_line_.size())
                {
                    cmd_line_.erase(cursor_pos_, 1);
                }
            }
            break;
        case VK_RETURN:
        case '\n':
            ret = s_console.executeCommand(cmd_line_.c_str());

            WriteConsole(console_wnd, &newl,  sizeof(newl),  written, NULL);
            WriteConsole(console_wnd, ret.c_str(), ret.size(),  written,  NULL);
            WriteConsole(console_wnd, &newl,  sizeof(newl),  written, NULL);            

            cmd_line_ = "";
            cursor_pos_ = 0;
            break;
        default:
            cmd_line_.insert(cursor_pos_, 1, (char)ch);
            ++cursor_pos_;

            char c = (char)ch;
            WriteConsole(console_wnd, &c,  sizeof(c),  written, NULL);
        }
    }
#endif

}



#ifndef _WIN32
//------------------------------------------------------------------------------
int ConsoleApp::setFlag(int fd, int flags)
{
    int val;

    if ( (val = fcntl(fd, F_GETFL, 0)) < 0 )
    {
        printf("fcntl F_GETFL error!\n");
        return -1;
    }

    val |= flags;

    if ( fcntl(fd, F_SETFL, val) < 0 )
    {
        printf("fcntl F_SETFL error!\n");
        return -2;
    }

    return 0;    
}
#endif

//------------------------------------------------------------------------------
void ConsoleApp::printCmdLine(unsigned cursor_pos, const std::string & cmd_line)
{
#ifndef _WIN32
    std::cout << "\r" << CTRL_ERASE_LINE;
    std::cout << cmd_line.c_str();
    int move_amount = (int)cmd_line.size()-(int)cursor_pos;
    if (move_amount)
    {
        assert(move_amount>0);
        std::cout << "\033[" << toString(move_amount) << "D";
    }    
#endif
}


//------------------------------------------------------------------------------
void ConsoleApp::deleteChar(unsigned & cursor_pos, std::string & cmd_line)
{
    if (cursor_pos != cmd_line.size())
    {
        cmd_line.erase(cursor_pos, 1);
        printCmdLine(cursor_pos, cmd_line);
    }
}


